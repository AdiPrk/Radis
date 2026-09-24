#pragma once

// Stable 64-bit identity of a cooked asset, derived from its source key. The same source gets
// the same ID on every machine and in every build, so assets can reference each other by ID.
// Sub-assets append "#name" to their file's key: "characters/knight.fbx#skeleton".
using AssetId = uint64_t;

// FNV-1a: a fixed algorithm, unlike std::hash, whose results can differ between compilers.
constexpr uint64_t Fnv1a64(std::string_view text)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (const char c : text)
    {
        hash = (hash ^ uint8_t(c)) * 0x100000001b3ull;
    }
    return hash;
}

constexpr AssetId MakeAssetId(std::string_view sourceKey) { return Fnv1a64(sourceKey); }

// The form of a source path that IDs are derived from: relative to the asset root, with forward
// slashes and ASCII lower case, so an ID doesn't change with the OS or the letter case of a path.
std::expected<std::string, std::string> SourceKey(const std::filesystem::path& source, const std::filesystem::path& root);