#pragma once

namespace Dog
{
    struct SimpleVertex
    {
        glm::vec3 position{ 0.f }; //Position of this vertex
        glm::vec3 color{ 1.f };    //Color of this vertex
        glm::vec3 normal{ 0.f };   //Normal of this vertex
        glm::vec2 uv{ 0.f };       //Texture coords of this vertex

        static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
    };

    // Main dispatch struct
    struct RHI
    {
        // lifecycle
        static bool (*Init)();
        static void (*Shutdown)();

        // mesh
        static void (*CreateVertexBuffers)(std::vector<SimpleVertex>& vertices, void* device);
        static void (*CreateIndexBuffers)(std::vector<uint32_t>& indices, void* device);

        static bool RHI_Init(GraphicsAPI backend);
    };


}
