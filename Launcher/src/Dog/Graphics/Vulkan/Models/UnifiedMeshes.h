/*****************************************************************//**
 * \file   UnifiedMeshes.hpp
 * \author Adi (aditya.prakash@digipen.edu)
 * \date   November 20 2024
 * \Copyright @ 2024 Digipen (USA) Corporation *

 * \brief  Yah
 *  *********************************************************************/

#pragma once

#include "Mesh.h"

namespace Dog
{
    // Forward reference
    class Device;
    class Buffer;

    struct MeshInfo {
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
    };

    class UnifiedMeshes
    {
    public:
        UnifiedMeshes();
        ~UnifiedMeshes() {}

        void AddMesh(
            Device& device,
            Mesh& mesh);

        Mesh& GetUnifiedMesh() { return unifiedMesh; }
        const MeshInfo& GetMeshInfo(uint32_t meshID) const { return mMeshInfos.at(meshID); }

    private:
        Mesh unifiedMesh;

        std::unordered_map<uint32_t, MeshInfo> mMeshInfos;
    };
}
