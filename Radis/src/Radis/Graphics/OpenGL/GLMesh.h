/*****************************************************************//**
 * \file   GLMesh.h
 * \brief  Definition of the GLMesh class for OpenGL mesh representation.
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

    class GLMesh : public IMesh {
    public:
        GLMesh(bool assignID = true);
        ~GLMesh();

        void CreateVertexBuffers(Device* device) override;
        void CreateIndexBuffers(Device* device) override;
        void DestroyBuffers() override;

        void Bind(VkCommandBuffer commandBuffer = nullptr) override;
        void Draw(VkCommandBuffer commandBuffer = nullptr, uint32_t baseIndex = 0) override;
    };
}

