#include <PCH/pch.h>
#include "UnifiedMesh.h"
#include "../Vulkan/Core/Device.h"
#include "../Vulkan/Core/Buffer.h"

namespace Dog
{
    UnifiedMeshes::UnifiedMeshes()
        : unifiedMesh(false)
    {
    }

    UnifiedMeshes::~UnifiedMeshes()
    {
    }

    void UnifiedMeshes::AddMesh(Device& device, IMesh& mesh)
    {
        unifiedMesh.mVertexBuffer.reset();
        unifiedMesh.mIndexBuffer.reset();

        MeshInfo meshInfo;
        meshInfo.indexCount = static_cast<uint32_t>(mesh.mIndices.size());
        meshInfo.firstIndex = static_cast<uint32_t>(unifiedMesh.mIndices.size());
        meshInfo.vertexOffset = static_cast<int32_t>(unifiedMesh.mVertices.size());

        unifiedMesh.mVertices.insert(unifiedMesh.mVertices.end(), mesh.mVertices.begin(), mesh.mVertices.end());
        unifiedMesh.mIndices.insert(unifiedMesh.mIndices.end(), mesh.mIndices.begin(), mesh.mIndices.end());

        unifiedMesh.CreateVertexBuffers(&device);
        unifiedMesh.CreateIndexBuffers(&device);

        mMeshInfos[mesh.mMeshID] = meshInfo;
    }

} // namespace Dog