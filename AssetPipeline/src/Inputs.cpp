#include <pch.h>
#include "Inputs.h"

static constexpr std::string_view kTextureExtensions[] = { ".png", ".jpg", ".jpeg", ".tga", ".hdr", ".exr", ".dds", ".ktx2" };
static constexpr std::string_view kModelExtensions[] = { ".glb", ".gltf", ".obj", ".fbx" };

static std::optional<AssetKind> ClassifyFile(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    if (std::ranges::contains(kTextureExtensions, ext)) return AssetKind::Texture;
    if (std::ranges::contains(kModelExtensions, ext))   return AssetKind::Model;
    return std::nullopt;
}

std::expected<std::vector<InputFile>, std::string> CollectInputs(const std::filesystem::path& input)
{
    std::error_code ec;

    if (std::filesystem::is_regular_file(input, ec))
    {
        const auto kind = ClassifyFile(input);
        if (!kind)
        {
            return std::unexpected(std::format("unsupported file type: {}", input.string()));
        }

        return std::vector<InputFile>{ { input, input.filename(), * kind } };
    }

    if (!std::filesystem::is_directory(input, ec))
    {
        return std::unexpected(std::format("not found: {}", input.string()));
    }

    std::vector<InputFile> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(input, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (!entry.is_regular_file())
            continue;

        // Side files like .bin and .mtl aren't inputs; the model that references them loads them.
        if (const auto kind = ClassifyFile(entry.path()))
        {
            files.push_back({ entry.path(), entry.path().lexically_relative(input), *kind });
        }
    }

    if (files.empty())
    {
        return std::unexpected(std::format("no textures or models found in {}", input.string()));
    }

    std::ranges::sort(files, {}, &InputFile::path);
    return files;
}