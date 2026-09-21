#include <pch.h>
#include "CommandLine.h"
#include "Inputs.h"
#include "Textures/Textures.h"
#include "Textures/DDS/DdsWriter.h"

static bool BuildTexture(const Options& opts, const InputFile& file)
{
    const auto start = std::chrono::steady_clock::now();

    const TextureCookSettings settings
    {
        .role = *opts.role,
        .target = opts.target,
        .quality = EncodeQuality::Best,
    };

    const auto texture = CookTexture(file.path, settings);
    if (!texture)
    {
        std::fprintf(stderr, "error: %s: %s\n", file.relative.string().c_str(), texture.error().c_str());
        return false;
    }

    std::filesystem::path outPath = opts.output / file.relative;
    outPath.replace_extension(".dds");

    if (auto written = WriteDds(outPath, *texture); !written)
    {
        std::fprintf(stderr, "error: %s\n", written.error().c_str());
        return false;
    }

    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::printf("%s -> %s (%s, %ux%u, %.1f ms)\n", file.relative.string().c_str(), outPath.string().c_str(),
        GetFormatInfo(texture->format).name, texture->width, texture->height, ms);

    return true;
}

int main(int argc, char** argv)
{
    const auto opts = ParseCommandLine(argc, argv);
    if (!opts)
    {
        return opts.error();
    }

    const auto inputs = CollectInputs(opts->input);
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