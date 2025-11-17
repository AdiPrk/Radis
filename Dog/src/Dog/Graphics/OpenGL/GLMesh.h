#pragma once

#include "Graphics/RHI/IMesh.h"

namespace Dog
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

        void Bind(VkCommandBuffer commandBuffer, VkBuffer instBuffer) override;
        void Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex = 0) override;

    public:
        GLuint mVAO{};
        GLuint mVBO{};
        GLuint mEBO{};
        GLuint mIVBO{};
    };
}

