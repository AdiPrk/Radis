#pragma once

#include "ImportedScene.h"
#include "../Textures/TextureCookQueue.h"

struct MaterialContext
{
    const ImportedScene& scene;
    std::string_view                 modelKey;     // for naming embedded images
    const std::filesystem::path& root;         // asset root, for image keys
    TextureCookQueue& textures;
    std::vector<std::string>& warnings;
    std::unordered_set<std::string>& usedImages;   // receives the key of every image file used
};

// Converts a material to the file's canonical layout and queues a texture request for every
// texture it uses. Requests are named after their images and recipe, so materials that use the
// same images the same way share one cooked texture.
std::expected<ModelFile::Material, std::string> BuildMaterial(const ImportedMaterial& material, MaterialContext& ctx);