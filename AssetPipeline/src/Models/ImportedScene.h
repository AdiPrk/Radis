#pragma once

#include "ModelFormat.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// What the importer hands to the rest of the model pipeline: plain data with no assimp types, in
// engine space (see ModelFormat.h) except for UVs, which are still bottom-left (OpenGL style) so
// tangents are generated to match +Y normal maps. MeshProcessing flips them for the file.

struct ImportedWeight
{
    uint32_t vertex = 0;
    float    weight = 0.0f;
};

// A joint that deforms a skinned mesh, and the vertices it moves.
struct ImportedBone
{
    uint32_t                    joint = 0;          // index into ImportedSkeleton::joints
    glm::mat4                   offset{ 1.0f };     // from the mesh's space to the joint's, at bind time
    std::vector<ImportedWeight> weights;
};

struct ImportedMesh
{
    std::string               name;
    uint32_t                  material = 0;
    std::vector<glm::vec3>    positions;
    std::vector<glm::vec3>    normals;
    std::vector<glm::vec2>    uvs;       // empty if the mesh has none
    std::vector<glm::vec4>    colors;    // empty if the mesh has none
    std::vector<uint32_t>     indices;   // triangle list
    std::vector<ImportedBone> bones;     // empty if the mesh isn't skinned
};

// A mesh placed by a node. `transform` already includes the conversion to engine space.
struct ImportedInstance
{
    uint32_t  mesh = 0;
    glm::mat4 transform{ 1.0f };
    int32_t   joint = -1;   // the nearest joint at or above the node, which a mesh without bones follows rigidly
};

// The node transforms are in engine space: a root's local transform includes the conversion.
struct ImportedJoint
{
    std::string name;
    int32_t     parent = -1;        // always less than the joint's own index
    glm::mat4   local{ 1.0f };      // rest pose, relative to the parent
    glm::mat4   global{ 1.0f };     // rest pose, relative to the model
};

// Every node a bone names, and their ancestors, in depth-first order. Empty for a model without bones.
struct ImportedSkeleton
{
    std::vector<ImportedJoint> joints;
};

// A file's whole node tree, as the file stores it, for sampling its animations.
struct ImportedNode
{
    std::string name;
    int32_t     parent = -1;       // always less than the node's own index
    glm::mat4   local{ 1.0f };     // relative to the parent, not converted to engine space
};

template <typename T>
struct ImportedKey
{
    float time = 0.0f;   // seconds
    T     value{};
};

// The keys of one node. An empty list leaves that part of the node's transform as stored.
struct ImportedChannel
{
    uint32_t                            node = 0;
    std::vector<ImportedKey<glm::vec3>> translations;
    std::vector<ImportedKey<glm::quat>> rotations;
    std::vector<ImportedKey<glm::vec3>> scales;
};

struct ImportedAnimation
{
    std::string                  name;
    float                        duration = 0.0f;     // seconds
    float                        sampleRate = 30.0f;  // frames per second, from the spacing of the keys
    std::vector<ImportedChannel> channels;
};

// The animations in a file, with the nodes they move. `conversion` takes the roots to engine space.
struct ImportedAnimations
{
    glm::mat4                      conversion{ 1.0f };
    std::vector<ImportedNode>      nodes;
    std::vector<ImportedAnimation> animations;
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
    ImportedSkeleton                    skeleton;
    ImportedAnimations                  animations;
    std::vector<ImportedMaterial>       materials;
    std::vector<std::vector<std::byte>> embeddedImages;   // encoded files (PNG, JPEG, ...)
    std::vector<std::string>            warnings;
};