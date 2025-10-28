#include <PCH/pch.h>
#include "Mesh.h"

#include "../Core/Device.h"
#include "../Core/Buffer.h"
#include "../Uniform/ShaderTypes.h"

namespace Dog
{
    VKMesh::VKMesh(bool assignID)
        : IMesh(assignID)
    {
    }

    void VKMesh::CreateVertexBuffers(Device* device)
    {
        mVertexCount = static_cast<uint32_t>(mVertices.size());
        mTriangleCount = mVertexCount / 3;
        //assert(vertexCount >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(mVertices[0]) * mVertexCount;
        uint32_t vertexSize = sizeof(mVertices[0]);

        Buffer stagingBuffer{
            *device,
            vertexSize,
            mVertexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY,
        };

        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)mVertices.data());

        mVertexBuffer = std::make_unique<Buffer>(
            *device,
            vertexSize,
            mVertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

        stagingBuffer.CopyBuffer(stagingBuffer.GetBuffer(), mVertexBuffer->GetBuffer(), bufferSize);
    }

    void VKMesh::CreateIndexBuffers(Device* device)
    {
        mIndexCount = static_cast<uint32_t>(mIndices.size());
        mHasIndexBuffer = mIndexCount > 0;

        if (!mHasIndexBuffer) {
            return;
        }

        VkDeviceSize bufferSize = sizeof(mIndices[0]) * mIndexCount;
        uint32_t indexSize = sizeof(mIndices[0]);

        Buffer stagingBuffer{
            *device,
            indexSize,
            mIndexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY,
        };

        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)mIndices.data());

        mIndexBuffer = std::make_unique<Buffer>(
            *device,
            indexSize,
            mIndexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

        stagingBuffer.CopyBuffer(stagingBuffer.GetBuffer(), mIndexBuffer->GetBuffer(), bufferSize);
    }

    void VKMesh::Cleanup()
    {
        mVertexBuffer.reset();
        mIndexBuffer.reset();
    }

    void VKMesh::Bind(VkCommandBuffer commandBuffer, VkBuffer instBuffer)
    {
        if (!mHasIndexBuffer)
        {
            DOG_CRITICAL("Binding but no index buffer! Should not be happening");
            return;
        }

        VkBuffer buffers[] = { mVertexBuffer->GetBuffer(), instBuffer };
        VkDeviceSize offsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, buffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }

    void VKMesh::Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex)  
    {
        if (!mHasIndexBuffer)
        {
            DOG_CRITICAL("Drawing with no index buffer! Should not be happening");
            return;
        }

        vkCmdDrawIndexed(commandBuffer, mIndexCount, 1, 0, 0, baseIndex);
    }

    std::vector<VkVertexInputBindingDescription> Vertex::GetBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(2);

        //Set bind description data
        bindingDescriptions[0].binding = 0;                             
        bindingDescriptions[0].stride = sizeof(Vertex);                 
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; 

        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = sizeof(SimpleInstanceUniforms);
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

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

        // Per instance
        attributeDescriptions.push_back({ 6,  1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SimpleInstanceUniforms, model)});
        attributeDescriptions.push_back({ 7,  1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SimpleInstanceUniforms, model) + sizeof(float) * 4 });
        attributeDescriptions.push_back({ 8,  1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SimpleInstanceUniforms, model) + sizeof(float) * 8 });
        attributeDescriptions.push_back({ 9,  1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SimpleInstanceUniforms, model) + sizeof(float) * 12 });
        //attributeDescriptions.push_back({ 10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceUniforms, tint) });
        //attributeDescriptions.push_back({ 11, 1, VK_FORMAT_R32_UINT, offsetof(InstanceUniforms, textureIndex) });
        //attributeDescriptions.push_back({ 12, 1, VK_FORMAT_R32_UINT, offsetof(InstanceUniforms, boneOffset) });

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
