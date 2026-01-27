/*****************************************************************//**
 * \file   UnifiedMesh.h
 * \brief  Definition of the UnifiedMeshes class for combining multiple meshes into a single mesh.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Graphics/RHI/IMesh.h"

namespace Radis
{
    // Forward reference
    class Device;
    
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

        void AddMesh(Device& device, IMesh& mesh);

        std::unique_ptr<IMesh>& GetUnifiedMesh() { return mUnifiedMesh; }
        const MeshInfo& GetMeshInfo(uint32_t meshID) const { return mMeshInfos.at(meshID); }
        uint32_t GetMeshCount() const { return static_cast<uint32_t>(mMeshInfos.size()); }

    private:
        std::unique_ptr<IMesh> mUnifiedMesh;
        std::unordered_map<uint32_t, MeshInfo> mMeshInfos;
    };
}
