#pragma once

#include "Graphics/RHI/IMesh.h"

namespace Dog
{
    // Forward reference
    class Device;

    class GLMesh : public IMesh {
    public:
        GLMesh(bool assignID = true);

        void CreateVertexBuffers(Device* device);
        void CreateIndexBuffers(Device* device);

        void Bind(VkCommandBuffer commandBuffer, VkBuffer instBuffer);
        void Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex = 0);

    public:
        GLuint mVAO, mVBO, mEBO, mIVBO;
    };
}

