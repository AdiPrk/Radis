#pragma once

#include "Graphics/RHI/IMesh.h"

namespace Dog
{
    // Forward reference
    class Device;

    class VKMesh : public IMesh {
    public:
        VKMesh(bool assignID = true);

        void CreateVertexBuffers(Device* device);
        void CreateIndexBuffers(Device* device);
        void DestroyBuffers() override;

        void Bind(VkCommandBuffer commandBuffer, VkBuffer instBuffer);
        void Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex = 0);
    };
}
