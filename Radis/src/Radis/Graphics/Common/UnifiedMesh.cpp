#include <PCH/pch.h>
#include "UnifiedMesh.h"
#include "../OpenGL/GLMesh.h"
#include "../Vulkan/VKMesh.h"
#include "../Vulkan/Core/Device.h"
#include "../Vulkan/Core/Buffer.h"
#include "Engine.h"

namespace Radis
{
    UnifiedMeshes::UnifiedMeshes()
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            mUnifiedMesh = std::make_unique<VKMesh>(false);
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            mUnifiedMesh = std::make_unique<GLMesh>(false);
        }
    }

    UnifiedMeshes::~UnifiedMeshes()
    {
    }

    void UnifiedMeshes::AddMesh(Device& device, IMesh& mesh)
    {
        mUnifiedMesh->DestroyBuffers();

        MeshInfo meshInfo;
        meshInfo.indexCount = static_cast<uint32_t>(mesh.mIndices.size());
        meshInfo.firstIndex = static_cast<uint32_t>(mUnifiedMesh->mIndices.size());
        meshInfo.vertexOffset = static_cast<int32_t>(mUnifiedMesh->mVertices.size());

        mUnifiedMesh->mVertices.insert(mUnifiedMesh->mVertices.end(), mesh.mVertices.begin(), mesh.mVertices.end());
        mUnifiedMesh->mIndices.insert(mUnifiedMesh->mIndices.end(), mesh.mIndices.begin(), mesh.mIndices.end());

        mUnifiedMesh->CreateVertexBuffers(&device);
        mUnifiedMesh->CreateIndexBuffers(&device);

        mMeshInfos[mesh.mMeshID] = meshInfo;
    }

} // namespace Radis