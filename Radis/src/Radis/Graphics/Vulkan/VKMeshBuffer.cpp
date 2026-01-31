/*****************************************************************//**
 * \file   VKMeshBuffer.cpp
 * \brief  Vulkan implementation of IMeshBuffer.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "VKMeshBuffer.h"
#include "Core/Device.h"
#include "Core/Allocator.h"

namespace Radis
{
    VKMeshBuffer::~VKMeshBuffer()
    {
        Destroy();
    }

    void VKMeshBuffer::Upload(Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        if (vertices.empty())
        {
            RADIS_WARN("VKMeshBuffer::Upload called with empty vertices!");
            return;
        }

        // Destroy existing buffers if any
        Destroy();

        CreateVertexBuffer(device, vertices);

        if (!indices.empty())
        {
            CreateIndexBuffer(device, indices);
        }

        mIsUploaded = true;
    }

    void VKMeshBuffer::CreateVertexBuffer(Device* device, const std::vector<Vertex>& vertices)
    {
        mVertexCount = static_cast<uint32_t>(vertices.size());
        const VkDeviceSize bufferSize = sizeof(vertices[0]) * mVertexCount;

        // Create staging buffer (CPU-visible)
        Buffer staging{};
        Allocator::CreateBuffer(
            staging,
            bufferSize,
            VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
        Allocator::SetAllocationName(staging.allocation, "Vertex Buffer Staging");

        if (!staging.mapping)
        {
            RADIS_CRITICAL("Failed to map vertex buffer staging memory!");
            return;
        }
        memcpy(staging.mapping, vertices.data(), static_cast<size_t>(bufferSize));

        // Create device-local vertex buffer
        Allocator::CreateBuffer(
            mVertexBuffer,
            bufferSize,
            VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT_KHR |
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR |
            VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        Allocator::SetAllocationName(mVertexBuffer.allocation, "Vertex Buffer");

        // Copy from staging to GPU buffer
        device->CopyBuffer(staging.buffer, mVertexBuffer.buffer, bufferSize);

        // Destroy staging buffer
        Allocator::DestroyBuffer(staging);
    }

    void VKMeshBuffer::CreateIndexBuffer(Device* device, const std::vector<uint32_t>& indices)
    {
        mIndexCount = static_cast<uint32_t>(indices.size());
        mHasIndexBuffer = mIndexCount > 0;

        if (!mHasIndexBuffer) return;

        const VkDeviceSize bufferSize = sizeof(indices[0]) * mIndexCount;

        // Create staging buffer (CPU-visible)
        Buffer staging{};
        Allocator::CreateBuffer(
            staging,
            bufferSize,
            VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
        Allocator::SetAllocationName(staging.allocation, "Index Buffer Staging");

        if (!staging.mapping)
        {
            RADIS_CRITICAL("Failed to map index buffer staging memory!");
            return;
        }
        memcpy(staging.mapping, indices.data(), static_cast<size_t>(bufferSize));

        // Create device-local index buffer
        Allocator::CreateBuffer(
            mIndexBuffer,
            bufferSize,
            VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT_KHR |
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR |
            VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        Allocator::SetAllocationName(mIndexBuffer.allocation, "Index Buffer");

        // Copy staging ¨ GPU
        device->CopyBuffer(staging.buffer, mIndexBuffer.buffer, bufferSize);

        Allocator::DestroyBuffer(staging);
    }

    void VKMeshBuffer::Destroy()
    {
        if (mVertexBuffer.buffer)
        {
            Allocator::DestroyBuffer(mVertexBuffer);
        }
        if (mHasIndexBuffer && mIndexBuffer.buffer)
        {
            Allocator::DestroyBuffer(mIndexBuffer);
        }

        mVertexBuffer = {};
        mIndexBuffer = {};
        mVertexCount = 0;
        mIndexCount = 0;
        mHasIndexBuffer = false;
        mIsUploaded = false;
    }

    void VKMeshBuffer::Bind(VkCommandBuffer cmd)
    {
        if (!cmd)
        {
            RADIS_CRITICAL("No command buffer passed to VKMeshBuffer::Bind!");
            return;
        }

        if (!mHasIndexBuffer)
        {
            RADIS_CRITICAL("VKMeshBuffer::Bind called but no index buffer!");
            return;
        }

        VkBuffer buffers[] = { mVertexBuffer.buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(cmd, mIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void VKMeshBuffer::Draw(VkCommandBuffer cmd, uint32_t instanceBase)
    {
        if (!mHasIndexBuffer)
        {
            RADIS_CRITICAL("VKMeshBuffer::Draw called but no index buffer!");
            return;
        }

        vkCmdDrawIndexed(cmd, mIndexCount, 1, 0, 0, instanceBase);
    }
}