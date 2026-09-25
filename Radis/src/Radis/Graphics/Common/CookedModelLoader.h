/*****************************************************************//**
 * \file   CookedModelLoader.h
 * \brief  Loads models cooked by the asset pipeline.
 *********************************************************************/

#pragma once

#include "ModelFormat.h"

namespace Radis
{
    class Mesh;

    // A cooked model (see ModelFormat.h) with every section decoded.
    struct CookedModelData
    {
        ModelFile::Header                        header{};
        std::vector<ModelFile::Submesh>          submeshes;
        std::vector<ModelFile::Material>         materials;
        std::vector<ModelFile::Position>         positions;
        std::vector<ModelFile::VertexAttributes> attributes;
        std::vector<uint32_t>                    indices;   // relative to each submesh's baseVertex; 16-bit files are widened
        std::vector<char>                        strings;   // texture file names, null-terminated

        // A material slot's texture file name, or nullptr for none.
        const char* TextureName(uint32_t offset) const { return offset == ModelFile::kNoTexture ? nullptr : strings.data() + offset; }
    };

    class CookedModelLoader
    {
    public:
        // Reads, decodes and validates a cooked model. Errors are logged.
        static bool Load(const std::string& path, CookedModelData& out);

        // One Mesh per submesh, with its material. Texture file names are resolved in
        // `textureDirectory`. Skinning isn't cooked yet, so bone data is left empty.
        static std::vector<std::unique_ptr<Mesh>> CreateMeshes(const CookedModelData& data, const std::filesystem::path& textureDirectory);
    };
}