/*****************************************************************//**
 * \file   VKMesh.h
 * \brief  Definition of the VKMesh class, a Vulkan implementation of IMesh.
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

    class VKMesh : public IMesh {
    public:
        VKMesh(bool assignID = true);
        ~VKMesh();

        void CreateVertexBuffers(Device* device);
        void CreateIndexBuffers(Device* device);
        void DestroyBuffers() override;

        void Bind(VkCommandBuffer commandBuffer);
        void Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex = 0);
    };
}
