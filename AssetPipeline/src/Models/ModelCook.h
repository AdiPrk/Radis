#pragma once

#include "../Textures/TextureCookQueue.h"

// Where cooked models are written: one flat folder (see OutputLayout.h), named after their source file.
std::filesystem::path ModelOutputPath(const std::filesystem::path& outputDir, const std::filesystem::path& source);

struct ModelCookContext
{
    const std::filesystem::path& outputDir;
    TextureCookQueue& textures;     // receives the textures the model's materials use
    std::unordered_set<std::string>& usedImages;   // receives the PathKey of every image file they use
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
ModelCookResult CookModel(const std::filesystem::path& source, ModelCookContext& ctx);