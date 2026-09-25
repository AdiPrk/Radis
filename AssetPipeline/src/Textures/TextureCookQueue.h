#pragma once

#include "Textures.h"

// What cooking one texture produced. The pixel data is written and released inside the job, so
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

struct QueuedTexture
{
    TextureRequest request;
    std::string    fileName;   // "<name>.dds", unique in its folder ignoring case
    std::string    nameNote;   // why the preferred name wasn't used, if it wasn't
};

// Collects the texture requests of every asset in a build, so each distinct texture is cooked once.
class TextureCookQueue
{
public:
    // Returns the texture's file name. A repeated key gets the name it already has, and the same key
    // with different settings is an error. A name another texture in the same folder already has gets
    // a numeric suffix.
    std::expected<std::string, std::string> Add(TextureRequest request);

    // Cooks and writes every queued texture, one after another. Result i belongs to Textures()[i].
    std::vector<TextureCookResult> Cook(const TextureBuildOptions& options, const std::filesystem::path& outputDir) const;

    std::span<const QueuedTexture> Textures() const { return m_textures; }

private:
    std::string UniqueFileName(const std::filesystem::path& folder, const std::string& name, std::string& note);

    std::vector<QueuedTexture>              m_textures;
    std::unordered_map<std::string, size_t> m_indexByKey;
    std::unordered_set<std::string>         m_takenNames;   // "folder/name", lower case as Windows compares file names
};