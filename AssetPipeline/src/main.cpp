#include <pch.h>
#include "CommandLine.h"
#include "Inputs.h"
#include "AssetId.h"
#include "Models/ModelCook.h"
#include "Models/ModelDump.h"
#include "Textures/TextureCookQueue.h"

static TextureRequest MakeTextureRequest(std::string key, const std::filesystem::path& path, TextureRole role)
{
    TextureRequest request;
    request.id = MakeAssetId(key);
    request.name = std::move(key);
    request.inputs.push_back({ .path = path });
    request.settings.role = role;
    return request;
}

static bool ReportModel(const std::string& name, const ModelCookResult& result)
{
    for (const std::string& warning : result.warnings)
    {
        std::fprintf(stderr, "warning: %s: %s\n", name.c_str(), warning.c_str());
    }

    if (!result.error.empty())
    {
        std::fprintf(stderr, "error: %s: %s\n", name.c_str(), result.error.c_str());
        return false;
    }

    std::printf("%s -> %s (%u submeshes, %u materials, %u vertices, %u triangles, %.1f ms)\n", name.c_str(), result.output.string().c_str(),
        result.submeshes, result.materials, result.vertices, result.triangles, result.milliseconds);
    return true;
}

// Prints the results once cooking is done, in request order.
static uint32_t ReportTextures(const TextureCookQueue& queue, std::span<const TextureCookResult> results)
{
    uint32_t failed = 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        const char* name = queue.Requests()[i].name.c_str();
        const TextureCookResult& result = results[i];

        for (const std::string& warning : result.warnings)
        {
            std::fprintf(stderr, "warning: %s: %s\n", name, warning.c_str());
        }

        if (!result.error.empty())
        {
            std::fprintf(stderr, "error: %s: %s\n", name, result.error.c_str());
            ++failed;
            continue;
        }

        std::printf("%s -> %s (%s, %ux%u, %u mips, %.1f ms)\n", name, result.output.string().c_str(),
            GetFormatInfo(result.format).name, result.width, result.height, result.mipCount, result.milliseconds);
    }
    return failed;
}

int main(int argc, char** argv)
{
    const auto opts = ParseCommandLine(argc, argv);
    if (!opts)
    {
        return opts.error();
    }

    if (!opts->dump.empty())
    {
        const auto dumped = DumpModel(opts->dump);
        if (!dumped)
        {
            std::fprintf(stderr, "error: %s: %s\n", opts->dump.string().c_str(), dumped.error().c_str());
        }
        return dumped ? 0 : 1;
    }

    const auto inputs = CollectInputs(opts->input, opts->output);
    if (!inputs)
    {
        std::fprintf(stderr, "error: %s\n", inputs.error().c_str());
        return 2;
    }

    const auto start = std::chrono::steady_clock::now();
    uint32_t   failed = 0;

    // Models first: their materials decide how the images they use are cooked.
    TextureCookQueue                textures;
    std::unordered_set<std::string> modelImages;
    ModelCookContext                modelContext{ opts->root, opts->output, textures, modelImages };
    for (const InputFile& file : *inputs)
    {
        if (file.kind != AssetKind::Model)
            continue;

        const auto key = SourceKey(file.path, opts->root);
        if (!key)
        {
            std::fprintf(stderr, "error: %s: %s\n", file.relative.string().c_str(), key.error().c_str());
            ++failed;
            continue;
        }
        failed += ReportModel(*key, CookModel(file.path, *key, modelContext)) ? 0 : 1;
    }

    // Then the textures no model uses, which need --role. Without one they're skipped: model folders
    // often hold images their materials don't use.
    uint32_t skipped = 0;
    for (const InputFile& file : *inputs)
    {
        if (file.kind != AssetKind::Texture)
            continue;

        auto key = SourceKey(file.path, opts->root);
        if (key && modelImages.contains(*key))
            continue;

        if (!opts->role)
        {
            ++skipped;
            continue;
        }

        const auto added = key.and_then([&](std::string& k) { return textures.Add(MakeTextureRequest(std::move(k), file.path, *opts->role)); });
        if (!added)
        {
            std::fprintf(stderr, "error: %s: %s\n", file.relative.string().c_str(), added.error().c_str());
            ++failed;
        }
    }

    if (skipped > 0)
    {
        std::fprintf(stderr, "warning: skipped %u textures no model uses; cook them with --role color|linear|normal|mask\n", skipped);
    }

    const TextureBuildOptions buildOptions
    {
        .target = opts->platform.target,
        .quality = opts->quality,
        .requireAlignedTopMip = opts->platform.requireAlignedTopMip,
    };
    failed += ReportTextures(textures, textures.Cook(buildOptions, opts->output));

    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("%zu inputs, %u failed, %.2f s\n", inputs->size(), failed, seconds);
    return failed ? 1 : 0;
}