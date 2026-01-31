/*****************************************************************//**
 * \file   IMeshBuffer.cpp
 * \brief  Implementation of IMeshBuffer factory and Vertex methods.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "IMeshBuffer.h"
#include "Engine.h"
#include "Graphics/Vulkan/VKMeshBuffer.h"
#include "Graphics/OpenGL/GLMeshBuffer.h"

namespace Radis
{
    std::unique_ptr<IMeshBuffer> CreateMeshBuffer()
    {
        switch (Engine::GetGraphicsAPI())
        {
        case GraphicsAPI::Vulkan:
            return std::make_unique<VKMeshBuffer>();
        case GraphicsAPI::OpenGL:
            return std::make_unique<GLMeshBuffer>();
        default:
            RADIS_CRITICAL("Unknown graphics API!");
            return nullptr;
        }
    }

    std::vector<VkVertexInputBindingDescription> Vertex::GetBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
        attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) });
        attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
        attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });
        attributeDescriptions.push_back({ 4, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(Vertex, boneIDs) });
        attributeDescriptions.push_back({ 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weights) });
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
        }
    }
}