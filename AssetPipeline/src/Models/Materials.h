#pragma once

#include "ImportedScene.h"
#include "../Textures/TextureCookQueue.h"

struct MaterialContext
{
    const ImportedScene& scene;
    std::string_view                 modelKey;     // PathKey of the model, to identify its embedded images
    std::string_view                 modelName;    // the model's name, for the textures it generates
    TextureCookQueue& textures;
    std::vector<std::string>& warnings;
    std::unordered_set<std::string>& usedImages;   // receives the PathKey of every image file used
};

// A material in the file's layout, with its textures as file names; the writer turns those into
// offsets into the model's string table.
struct BuiltMaterial
{
    ModelFile::Material                                       material{};
    std::array<std::string, ModelFile::kMaterialTextureCount> textures;   // empty for none
};

// Converts a material to the file's canonical layout and queues a texture for every slot it uses.
// A texture made from one image file is named after that file; anything else is named
// <model>_<material>_<slot>. Materials using the same images the same way share one texture.
std::expected<BuiltMaterial, std::string> BuildMaterial(const ImportedMaterial& material, MaterialContext& ctx);