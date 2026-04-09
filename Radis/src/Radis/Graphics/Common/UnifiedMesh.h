/*****************************************************************//**
 * \file   UnifiedMesh.h
 * \brief  Definition of the UnifiedMeshes class for combining multiple meshes.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Graphics/RHI/Mesh.h"

namespace Radis
{
    class Device;
    class Model;

    struct MeshInfo {
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
    };

    class UnifiedMeshes
    {
    public:
        UnifiedMeshes();
        ~UnifiedMeshes();

        void AddMesh(Device& device, const Mesh& mesh);
        void AddModels(Device& device, const std::vector<std::unique_ptr<Model>>& models);

        Mesh& GetUnifiedMesh() { return mUnifiedMesh; }
        const Mesh& GetUnifiedMesh() const { return mUnifiedMesh; }
        const MeshInfo& GetMeshInfo(uint32_t meshID) const { return mMeshInfos.at(meshID); }
        uint32_t GetMeshCount() const { return static_cast<uint32_t>(mMeshInfos.size()); }

    private:
        Mesh mUnifiedMesh;
        std::unordered_map<uint32_t, MeshInfo> mMeshInfos;
    };
}