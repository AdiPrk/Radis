#include <PCH/pch.h>
#include "UnifiedMeshes.h"
#include "Mesh.h"
#include "../Core/Device.h"
#include "ModelLibrary.h"
#include "../Texture/TextureLibrary.h"

namespace Dog
{
    UnifiedMeshes::UnifiedMeshes()
        : unifiedMesh(false)
    {
    }

    void UnifiedMeshes::AddMesh(Device& device, Mesh& mesh)
    {
        unifiedMesh.vertexBuffer.reset();
        unifiedMesh.indexBuffer.reset();

        MeshInfo meshInfo;
        meshInfo.indexCount = static_cast<uint32_t>(mesh.indices.size());
        meshInfo.firstIndex = static_cast<uint32_t>(unifiedMesh.indices.size());
        meshInfo.vertexOffset = static_cast<int32_t>(unifiedMesh.vertices.size());

        unifiedMesh.vertices.insert(unifiedMesh.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
        unifiedMesh.indices.insert(unifiedMesh.indices.end(), mesh.indices.begin(), mesh.indices.end());

        unifiedMesh.createVertexBuffers(device);
        unifiedMesh.createIndexBuffers(device);

        mMeshInfos[mesh.mMeshID] = meshInfo;
    }

}

