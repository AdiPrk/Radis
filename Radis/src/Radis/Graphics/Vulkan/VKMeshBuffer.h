/*****************************************************************//**
 * \file   VKMeshBuffer.h
 * \brief  Vulkan implementation of IMeshBuffer.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Graphics/RHI/IMeshBuffer.h"

namespace Radis
{
    class VKMeshBuffer : public IMeshBuffer
    {
    public:
        VKMeshBuffer() = default;
        ~VKMeshBuffer() override;

        void Upload(Device* device,
            const std::vector<Vertex>& vertices,
            const std::vector<uint32_t>& indices) override;
        void Destroy() override;

        void Bind(VkCommandBuffer cmd) override;
        void Draw(VkCommandBuffer cmd, uint32_t instanceBase = 0) override;

        // Vulkan-specific accessors
        Buffer& GetVertexBuffer() override { return mVertexBuffer; }
        Buffer& GetIndexBuffer() override { return mIndexBuffer; }
        const Buffer& GetVertexBuffer() const override { return mVertexBuffer; }
        const Buffer& GetIndexBuffer() const override { return mIndexBuffer; }

    private:
        void CreateVertexBuffer(Device* device, const std::vector<Vertex>& vertices);
        void CreateIndexBuffer(Device* device, const std::vector<uint32_t>& indices);

        Buffer mVertexBuffer{};
        Buffer mIndexBuffer{};
    };
}