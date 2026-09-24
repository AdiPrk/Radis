#pragma once

#include "Textures.h"

// Where a cooked texture is written. Named by ID, so references between assets never depend on
// source paths, and the runtime can find a texture from its ID alone.
std::filesystem::path TextureOutputPath(const std::filesystem::path& outputDir, AssetId id);

// What cooking one request produced. The pixel data is written and released inside the job, so
// results stay small however many textures a build cooks.
struct TextureCookResult
{
    std::filesystem::path    output;
    std::string              error;      // empty on success
    std::vector<std::string> warnings;
    TextureFormat            format = TextureFormat::Unknown;
    uint32_t                 width = 0;
    uint32_t                 height = 0;
    uint32_t                 mipCount = 0;
    double                   milliseconds = 0.0;
};

// Collects the texture requests of every asset in a build, so each distinct texture is cooked once.
class TextureCookQueue
{
public:
    // A repeated request for a queued texture is merged. The same ID with a different source
    // (a hash collision) or different settings is an error.
    std::expected<void, std::string> Add(TextureRequest request);

    // Cooks and writes every queued texture, one after another. Result i belongs to Requests()[i].
    std::vector<TextureCookResult> Cook(const TextureBuildOptions& options, const std::filesystem::path& outputDir) const;

    std::span<const TextureRequest> Requests() const { return m_requests; }

private:
    std::vector<TextureRequest>         m_requests;
    std::unordered_map<AssetId, size_t> m_indexById;
};