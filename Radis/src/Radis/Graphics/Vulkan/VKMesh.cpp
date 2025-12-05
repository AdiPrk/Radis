#include <PCH/pch.h>
#include "VKMesh.h"

#include "Core/Device.h"
#include "Core/Buffer.h"
#include "Uniform/ShaderTypes.h"

namespace Radis
{
    VKMesh::VKMesh(bool assignID)
        : IMesh(assignID)
    {
    }

    VKMesh::~VKMesh()
    {
        DestroyBuffers();
    }

    Buffer mVertexBuffer;
    Buffer mIndexBuffer;

    void VKMesh::CreateVertexBuffers(Device* device)
    {
        mVertexCount = static_cast<uint32_t>(mVertices.size());
        if (mVertexCount == 0) return;

        const VkDeviceSize bufferSize = sizeof(mVertices[0]) * mVertexCount;

        // -- Create staging buffer (CPU-visible) --
        Buffer staging{};
        Allocator::CreateBuffer(
            staging,
            bufferSize,
            VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
        Allocator::SetAllocationName(staging.allocation, "Vertex Buffer Staging");

        // Copy vertex data to staging buffer
        if (!staging.mapping)
        {
            RADIS_CRITICAL("Failed to map vertex buffer staging memory!");
            return;
        }
        memcpy(staging.mapping, mVertices.data(), static_cast<size_t>(bufferSize));

        // -- Create device-local vertex buffer --
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

        // destroy staging buffer
        Allocator::DestroyBuffer(staging);
    }

    void VKMesh::CreateIndexBuffers(Device* device)
    {
        mIndexCount = static_cast<uint32_t>(mIndices.size());
        mHasIndexBuffer = mIndexCount > 0;
        mTriangleCount = mIndexCount / 3;

        if (!mHasIndexBuffer) return;

        const VkDeviceSize bufferSize = sizeof(mIndices[0]) * mIndexCount;

        // -- Create staging buffer (CPU-visible) --
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
        memcpy(staging.mapping, mIndices.data(), static_cast<size_t>(bufferSize));

        // -- Create device-local index buffer --
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

    void VKMesh::DestroyBuffers()
    {
        Allocator::DestroyBuffer(mVertexBuffer);
        if (mHasIndexBuffer)
        {
            Allocator::DestroyBuffer(mIndexBuffer);
        }
    }

    void VKMesh::Bind(VkCommandBuffer commandBuffer)
    {
        if (!commandBuffer)
        {
            RADIS_CRITICAL("No command buffer passed to bind mesh!");
            return;
        }

        if (!mHasIndexBuffer)
        {
            RADIS_CRITICAL("Binding but no index buffer! Should not be happening");
            return;
        }

        VkBuffer buffers[] = { mVertexBuffer.buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void VKMesh::Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex)  
    {
        if (!mHasIndexBuffer)
        {
            RADIS_CRITICAL("Drawing with no index buffer! Should not be happening");
            return;
        }

        vkCmdDrawIndexed(commandBuffer, mIndexCount, 1, 0, 0, baseIndex);
    }

    std::vector<VkVertexInputBindingDescription> Vertex::GetBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);

        //Set bind description data
        bindingDescriptions[0].binding = 0;                             
        bindingDescriptions[0].stride = sizeof(Vertex);                 
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; 

        //bindingDescriptions[1].binding = 1;
        //bindingDescriptions[1].stride = sizeof(InstanceUniforms);
        //bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        //Return description
        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
    {
        //Create a vector of attribute descriptions
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        // Per vertex
        attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
        attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) });
        attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
        attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });
        attributeDescriptions.push_back({ 4, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(Vertex, boneIDs) });
        attributeDescriptions.push_back({ 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weights) });

        //Return description
        return attributeDescriptions;
    }

    void Vertex::SetBoneData(int boneID, float weight)
    {
        for (int i = 0; i < MAX_BONE_INFLUENCE; i++) 
        {
            if (weights[i] == 0.0f)
            {
                boneIDs[i] = boneID;
                weights[i] = weight;
                return;
            }
            if (i == MAX_BONE_INFLUENCE - 1) 
            {
                __debugbreak();
            }
        }
    }

}
