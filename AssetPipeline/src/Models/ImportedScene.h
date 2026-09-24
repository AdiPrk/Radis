#pragma once

#include "ModelFormat.h"

#include <glm/glm.hpp>

// What the importer hands to the rest of the model pipeline: plain data with no assimp types, in
// engine space (see ModelFormat.h) except for UVs, which are still bottom-left (OpenGL style) so
// tangents are generated to match +Y normal maps. MeshProcessing flips them for the file.

struct ImportedMesh
{
    std::string            name;
    uint32_t               material = 0;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;       // empty if the mesh has none
    std::vector<glm::vec4> colors;    // empty if the mesh has none
    std::vector<uint32_t>  indices;   // triangle list
};

// A mesh placed by a node. `transform` already includes the conversion to engine space.
struct ImportedInstance
{
    uint32_t  mesh = 0;
    glm::mat4 transform{ 1.0f };
};

// An image a material uses: a file next to the model, or one embedded in it.
struct ImportedTexture
{
    std::filesystem::path  path;            // an existing image file
    int32_t                embedded = -1;   // index into ImportedScene::embeddedImages
    ModelFile::TextureWrap wrapU = ModelFile::TextureWrap::Repeat;
    ModelFile::TextureWrap wrapV = ModelFile::TextureWrap::Repeat;

    explicit operator bool() const { return embedded >= 0 || !path.empty(); }
};

// The texture slots a source material can have, before they're combined into the file's slots.
enum class SourceSlot : uint8_t { BaseColor, Opacity, Normal, Occlusion, Roughness, Metalness, Emissive, Transmission, Count };

struct ImportedMaterial
{
    std::string          name;
    glm::vec4            baseColor{ 1.0f };
    glm::vec3            emissive{ 0.0f };
    float                emissiveStrength = 1.0f;
    float                metallic = 0.0f;
    float                roughness = 1.0f;
    float                occlusionStrength = 1.0f;
    float                normalScale = 1.0f;
    float                transmission = 0.0f;
    float                ior = 1.5f;
    float                alphaCutoff = 0.5f;
    ModelFile::AlphaMode alphaMode = ModelFile::AlphaMode::Opaque;
    bool                 alphaFromTexture = false;   // alphaMode comes from how the base color image uses its alpha
    bool                 doubleSided = false;

    std::array<ImportedTexture, size_t(SourceSlot::Count)> textures;

    const ImportedTexture& Texture(SourceSlot slot) const { return textures[size_t(slot)]; }
    ImportedTexture& Texture(SourceSlot slot) { return textures[size_t(slot)]; }
};

struct ImportedScene
{
    std::vector<ImportedMesh>           meshes;
    std::vector<ImportedInstance>       instances;
    std::vector<ImportedMaterial>       materials;
    std::vector<std::vector<std::byte>> embeddedImages;   // encoded files (PNG, JPEG, ...)
    std::vector<std::string>            warnings;
};