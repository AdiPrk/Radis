#include <pch.h>
#include "Inputs.h"

// .dds/.ktx2 come back once passthrough exists; the loaders can't read them yet.
static constexpr std::string_view kTextureExtensions[] = { ".png", ".jpg", ".jpeg", ".tga", ".hdr", ".exr" };
static constexpr std::string_view kModelExtensions[] = { ".glb", ".gltf", ".obj", ".fbx" };

static std::optional<AssetKind> ClassifyFile(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    if (std::ranges::contains(kTextureExtensions, ext)) return AssetKind::Texture;
    if (std::ranges::contains(kModelExtensions, ext))   return AssetKind::Model;
    return std::nullopt;
}

std::expected<std::vector<InputFile>, std::string> CollectInputs(const std::filesystem::path& input, const std::filesystem::path& exclude)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (fs::is_regular_file(input, ec))
    {
        const auto kind = ClassifyFile(input);
        if (!kind)
        {
            return std::unexpected(std::format("unsupported file type: {}", input.string()));
        }

        return std::vector<InputFile>{ { input, input.filename(), * kind } };
    }

    if (!fs::is_directory(input, ec))
    {
        return std::unexpected(std::format("not found: {}", input.string()));
    }

    // The error_code overloads throughout: a file vanishing mid-scan shouldn't throw.
    std::vector<InputFile> files;
    fs::recursive_directory_iterator it(input, fs::directory_options::skip_permission_denied, ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        const fs::directory_entry& entry = *it;
        std::error_code            entryEc;   // per-entry failures just skip the entry

        if (entry.is_directory(entryEc))
        {
            if (fs::equivalent(entry.path(), exclude, entryEc))
            {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (!entry.is_regular_file(entryEc))
            continue;

        // Side files like .bin and .mtl aren't inputs; the model that references them loads them.
        if (const auto kind = ClassifyFile(entry.path()))
        {
            files.push_back({ entry.path(), entry.path().lexically_relative(input), *kind });
        }
    }

    if (ec)
    {
        return std::unexpected(std::format("cannot scan {}: {}", input.string(), ec.message()));
    }

    if (files.empty())
    {
        return std::unexpected(std::format("no textures or models found in {}", input.string()));
    }

    std::ranges::sort(files, {}, &InputFile::path);
    return files;
}