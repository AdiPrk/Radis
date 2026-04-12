/*****************************************************************//**
 * \file   Mesh.cpp
 * \brief  Implementation of the Mesh class.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "Mesh.h"

namespace Radis
{
    int Mesh::uniqueMeshIndex = 0;

    Mesh::Mesh(bool assignID)
        : mMeshID(0)
        , mBuffer(nullptr)
    {
        if (assignID)
        {
            mMeshID = uniqueMeshIndex++;
        }
    }

    Mesh::~Mesh()
    {
        ReleaseGPU();
    }

    void Mesh::UploadToGPU(Device* device)
    {
        if (mVertices.empty())
        {
            RADIS_WARN("Mesh::UploadToGPU called with no vertices!");
            return;
        }

        // Create buffer if not exists
        if (!mBuffer)
        {
            mBuffer = CreateMeshBuffer();
        }

        // Upload data
        mBuffer->Upload(device, mVertices, mIndices);
    }

    void Mesh::ReleaseGPU()
    {
        if (mBuffer)
        {
            mBuffer->Destroy();
            mBuffer.reset();
        }
    }

    void Mesh::RecreateBuffer(Device* device)
    {
        // Simply release and re-upload
        ReleaseGPU();
        UploadToGPU(device);
    }

    void Mesh::Bind(VkCommandBuffer cmd)
    {
        if (mBuffer && mBuffer->IsUploaded())
        {
            mBuffer->Bind(cmd);
        }
        else
        {
            RADIS_WARN("Mesh::Bind called but buffer not uploaded!");
        }
    }

    void Mesh::Draw(VkCommandBuffer cmd, uint32_t instanceBase)
    {
        if (mBuffer && mBuffer->IsUploaded())
        {
            mBuffer->Draw(cmd, instanceBase);
        }
        else
        {
            RADIS_WARN("Mesh::Draw called but buffer not uploaded!");
        }
    }
}