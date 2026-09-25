/*****************************************************************//**
 * \file   CookedModelLoader.h
 * \brief  Loads models cooked by the asset pipeline.
 *********************************************************************/

#pragma once

#include "ModelFormat.h"

namespace Radis
{
    class Mesh;
    struct Skeleton;

    // A cooked model (see ModelFormat.h) with every section decoded.
    struct CookedModelData
    {
        ModelFile::Header                        header{};
        std::vector<ModelFile::Submesh>          submeshes;
        std::vector<ModelFile::Material>         materials;
        std::vector<ModelFile::Position>         positions;
        std::vector<ModelFile::VertexAttributes> attributes;
        std::vector<uint32_t>                    indices;   // relative to each submesh's baseVertex; 16-bit files are widened
        std::vector<char>                        strings;   // texture file names and joint names, null-terminated

        // Skinned models only.
        std::vector<ModelFile::Joint>            joints;
        std::vector<uint16_t>                    skinJoints;     // the joint each palette entry follows
        std::vector<ModelFile::Matrix3x4>        inverseBinds;   // one per palette entry
        std::vector<ModelFile::SkinWeights>      skin;           // one per vertex

        // A material slot's texture file name, or nullptr for none.
        const char* TextureName(uint32_t offset) const { return offset == ModelFile::kNoTexture ? nullptr : strings.data() + offset; }
        const char* JointName(size_t joint) const { return strings.data() + joints[joint].name; }
    };

    class CookedModelLoader
    {
    public:
        // Reads, decodes and validates a cooked model. Errors are logged.
        static bool Load(const std::string& path, CookedModelData& out);

        // One Mesh per submesh, with its material. Texture file names are resolved in
        // `textureDirectory`. A skinned vertex's bone IDs are palette entries.
        static std::vector<std::unique_ptr<Mesh>> CreateMeshes(const CookedModelData& data, const std::filesystem::path& textureDirectory);

        // The model's skeleton, or nullptr for a model that isn't skinned.
        static std::unique_ptr<Skeleton> CreateSkeleton(const CookedModelData& data);
    };
}