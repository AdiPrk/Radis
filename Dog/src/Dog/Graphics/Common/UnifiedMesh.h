#pragma once

#include "../Vulkan/VKMesh.h"

namespace Dog
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

        void AddMesh(
            Device& device,
            IMesh& mesh);

        VKMesh& GetUnifiedMesh() { return unifiedMesh; }
        const MeshInfo& GetMeshInfo(uint32_t meshID) const { return mMeshInfos.at(meshID); }

    private:
        VKMesh unifiedMesh;
        std::unordered_map<uint32_t, MeshInfo> mMeshInfos;
    };
}
