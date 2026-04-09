/*****************************************************************//**
 * \file   UnifiedMesh.cpp
 * \brief  Implementation of UnifiedMeshes class.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "UnifiedMesh.h"
#include "Model.h"
#include "../Vulkan/Core/Device.h"

namespace Radis
{
    UnifiedMeshes::UnifiedMeshes()
        : mUnifiedMesh(false)  // Don't assign ID to unified mesh
    {
    }

    UnifiedMeshes::~UnifiedMeshes()
    {
    }

    void UnifiedMeshes::AddMesh(Device& device, const Mesh& mesh)
    {
        // Release existing GPU buffer
        mUnifiedMesh.ReleaseGPU();

        // Track mesh info
        MeshInfo meshInfo;
        meshInfo.indexCount = static_cast<uint32_t>(mesh.mIndices.size());
        meshInfo.firstIndex = static_cast<uint32_t>(mUnifiedMesh.mIndices.size());
        meshInfo.vertexOffset = static_cast<int32_t>(mUnifiedMesh.mVertices.size());

        // Append CPU data
        mUnifiedMesh.mVertices.insert(mUnifiedMesh.mVertices.end(), mesh.mVertices.begin(), mesh.mVertices.end());
        mUnifiedMesh.mIndices.insert(mUnifiedMesh.mIndices.end(), mesh.mIndices.begin(), mesh.mIndices.end());

        // Re-upload to GPU
        mUnifiedMesh.UploadToGPU(&device);

        mMeshInfos[mesh.mMeshID] = meshInfo;
    }

    void UnifiedMeshes::AddModels(Device& device, const std::vector<std::unique_ptr<Model>>& models)
    {
        // Release existing GPU buffer
        mUnifiedMesh.ReleaseGPU();

        for (const auto& mo : models)
        for (const auto& m : mo->mMeshes)
        {
            // Track mesh info
            MeshInfo meshInfo;
            meshInfo.indexCount = static_cast<uint32_t>(m->mIndices.size());
            meshInfo.firstIndex = static_cast<uint32_t>(mUnifiedMesh.mIndices.size());
            meshInfo.vertexOffset = static_cast<int32_t>(mUnifiedMesh.mVertices.size());

            mUnifiedMesh.mVertices.insert(mUnifiedMesh.mVertices.end(), m->mVertices.begin(), m->mVertices.end());
            mUnifiedMesh.mIndices.insert(mUnifiedMesh.mIndices.end(), m->mIndices.begin(), m->mIndices.end());
            mMeshInfos[m->GetID()] = meshInfo;
        }

        // Re-upload to GPU
        mUnifiedMesh.UploadToGPU(&device);
    }
}