#pragma once

#include "../AssetId.h"
#include "../Textures/TextureCookQueue.h"

// Where a cooked model is written, named by ID like textures.
std::filesystem::path ModelOutputPath(const std::filesystem::path& outputDir, AssetId id);

struct ModelCookContext
{
    const std::filesystem::path& root;         // asset root, for texture keys
    const std::filesystem::path& outputDir;
    TextureCookQueue& textures;     // receives the textures the model's materials use
    std::unordered_set<std::string>& usedImages;   // receives the keys of the image files they use
};

struct ModelCookResult
{
    std::filesystem::path    output;
    std::string              error;       // empty on success
    std::vector<std::string> warnings;
    uint32_t                 submeshes = 0;
    uint32_t                 materials = 0;
    uint32_t                 vertices = 0;
    uint32_t                 triangles = 0;
    double                   milliseconds = 0.0;
};

// Imports, processes and writes one model. Its textures are only queued; they're cooked with the
// rest of the build's textures.
ModelCookResult CookModel(const std::filesystem::path& source, const std::string& key, ModelCookContext& ctx);