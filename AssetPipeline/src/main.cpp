#include <pch.h>
#include "CommandLine.h"
#include "Inputs.h"
#include "Textures/Textures.h"
#include "Textures/DDS/DdsWriter.h"

// Where a texture's cooked file goes, relative to the output directory.
static std::filesystem::path CookedTexturePath(const InputFile& file)
{
    return std::filesystem::path(file.relative).replace_extension(".dds");
}

// Sources that differ only by extension (or letter case, on case-insensitive file systems)
// would cook to the same file and silently overwrite each other.
static bool CheckOutputCollisions(std::span<const InputFile> inputs)
{
    std::unordered_map<std::string, const InputFile*> claimed;
    bool ok = true;

    for (const InputFile& file : inputs)
    {
        if (file.kind != AssetKind::Texture)
            continue;

        const std::filesystem::path cooked = CookedTexturePath(file);
        std::string key = cooked.generic_string();
        std::ranges::transform(key, key.begin(), [](unsigned char c) { return char(std::tolower(c)); });

        if (const auto [it, inserted] = claimed.try_emplace(std::move(key), &file); !inserted)
        {
            std::fprintf(stderr, "error: %s and %s both cook to %s\n", it->second->relative.string().c_str(),
                file.relative.string().c_str(), cooked.string().c_str());
            ok = false;
        }
    }
    return ok;
}

static bool BuildTexture(const Options& opts, const InputFile& file)
{
    const auto start = std::chrono::steady_clock::now();

    const TextureCookSettings settings
    {
        .role = *opts.role,
        .target = opts.platform.target,
        .quality = opts.quality,
        .requireAlignedTopMip = opts.platform.requireAlignedTopMip,
    };

    const auto texture = CookTexture(file.path, settings);
    if (!texture)
    {
        std::fprintf(stderr, "error: %s: %s\n", file.relative.string().c_str(), texture.error().c_str());
        return false;
    }

    for (const std::string& warning : texture->warnings)
    {
        std::fprintf(stderr, "warning: %s: %s\n", file.relative.string().c_str(), warning.c_str());
    }

    const std::filesystem::path outPath = opts.output / CookedTexturePath(file);
    if (auto written = WriteDds(outPath, *texture); !written)
    {
        std::fprintf(stderr, "error: %s\n", written.error().c_str());
        return false;
    }

    const double     ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const CookedMip& top = texture->mips.front();
    std::printf("%s -> %s (%s, %ux%u, %zu mips, %.1f ms)\n", file.relative.string().c_str(), outPath.string().c_str(),
        GetFormatInfo(texture->format).name, top.width, top.height, texture->mips.size(), ms);
    return true;
}

int main(int argc, char** argv)
{
    const auto opts = ParseCommandLine(argc, argv);
    if (!opts)
    {
        return opts.error();
    }

    const auto inputs = CollectInputs(opts->input, opts->output);
    if (!inputs)
    {
        std::fprintf(stderr, "error: %s\n", inputs.error().c_str());
        return 2;
    }

    const bool hasTextures = std::ranges::any_of(*inputs, [](const InputFile& f) { return f.kind == AssetKind::Texture; });
    if (hasTextures && !opts->role)
    {
        std::fprintf(stderr, "error: textures need --role color|linear|normal|mask\n");
        return 2;
    }

    if (!CheckOutputCollisions(*inputs))
    {
        return 2;
    }

    const auto start = std::chrono::steady_clock::now();
    uint32_t   failed = 0;

    for (const InputFile& file : *inputs)
    {
        switch (file.kind)
        {
        case AssetKind::Texture:
            failed += BuildTexture(*opts, file) ? 0 : 1;
            break;
        case AssetKind::Model:
            std::printf("skipped %s (models not implemented yet)\n", file.relative.string().c_str());
            break;
        }
    }

    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("%zu inputs, %u failed, %.2f s\n", inputs->size(), failed, seconds);
    return failed ? 1 : 0;
}