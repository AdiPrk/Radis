#pragma once

namespace Dog
{
    // Forward reference
    class Device;
    class Buffer;
    struct RenderingResource;

    struct Vertex
    {
        glm::vec3 position{0.f}; //Position of this vertex
        glm::vec3 color{1.f};    //Color of this vertex
        glm::vec3 normal{0.f};   //Normal of this vertex
        glm::vec2 uv{0.f};       //Texture coords of this vertex

        static constexpr int MAX_BONE_INFLUENCE = 4;

        std::array<int, MAX_BONE_INFLUENCE> boneIDs = { -1, -1, -1, -1 };
        std::array<float, MAX_BONE_INFLUENCE> weights = { 0.0f, 0.0f, 0.0f, 0.0f };

        static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();

        void SetBoneData(int boneID, float weight);
    };

    class Mesh {
    public:
        Mesh(bool assignID = true);

        void CreateVertexBuffers(Device& device);
        void CreateIndexBuffers(Device& device);

        void Bind(VkCommandBuffer commandBuffer, VkBuffer instBuffer);
        void Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex = 0);

        std::unique_ptr<Buffer> mVertexBuffer;
        uint32_t mVertexCount = 0;

        bool mHasIndexBuffer = false;
        std::unique_ptr<Buffer> mIndexBuffer;
        uint32_t mIndexCount = 0;
        uint32_t mTriangleCount = 0;

        std::vector<Vertex> mVertices{};
        std::vector<uint32_t> mIndices{};

        // Tex data if from memory
        std::unique_ptr<unsigned char[]> mTextureData = nullptr;
        uint32_t mTextureSize = 0;

        // unique index for this mesh
        uint32_t mMeshID = 0;

        // Path to textures
        bool loadedTextures = false;
        std::string diffuseTexturePath = "";
        uint32_t diffuseTextureIndex = 10001;
        
    private:
        static int GetTotalMeshCount() { return uniqueMeshIndex; }
        static int uniqueMeshIndex;
    };
}
