#include <pch.h>
#include "AssetId.h"

// Resolves the path as the file system sees it, so "Assets" and "assets" match on Windows and a
// root reached through a symlink or junction still contains its files.
static std::filesystem::path ResolvePath(const std::filesystem::path& path)
{
    std::error_code       ec;
    std::filesystem::path result = std::filesystem::weakly_canonical(path, ec);
    if (ec)
    {
        result = std::filesystem::absolute(path, ec).lexically_normal();
    }

    // "assets/" can keep an empty last element, which would throw off lexically_relative.
    if (!result.has_filename() && result.has_relative_path())
    {
        result = result.parent_path();
    }
    return result;
}

std::expected<std::string, std::string> SourceKey(const std::filesystem::path& source, const std::filesystem::path& root)
{
    const std::filesystem::path relative = ResolvePath(source).lexically_relative(ResolvePath(root));
    if (relative.empty() || relative == "." || *relative.begin() == "..")
    {
        return std::unexpected(std::format("{} is outside the asset root {}", source.string(), root.string()));
    }

    // UTF-8 keeps non-ASCII names identical on every OS; only ASCII letters change case.
    const std::u8string text = relative.generic_u8string();
    std::string         key(text.size(), '\0');
    std::ranges::transform(text, key.begin(), [](char8_t c) { return char(c >= u8'A' && c <= u8'Z' ? c + (u8'a' - u8'A') : c); });
    return key;
}