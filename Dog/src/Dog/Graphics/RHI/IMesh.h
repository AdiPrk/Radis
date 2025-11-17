#pragma once

#include "Graphics/RHI/RHI.h"
#include "Graphics/Vulkan/Core/AccelerationStructures.h"

namespace Dog
{
    class Device;

    struct Vertex
    {
        glm::vec3 position{ 0.f }; //Position of this vertex
        glm::vec3 color{ 1.f };    //Color of this vertex
        glm::vec3 normal{ 0.f };   //Normal of this vertex
        glm::vec2 uv{ 0.f };       //Texture coords of this vertex

        static constexpr int MAX_BONE_INFLUENCE = 4;

        std::array<int, MAX_BONE_INFLUENCE> boneIDs = { -1, -1, -1, -1 };
        std::array<float, MAX_BONE_INFLUENCE> weights = { 0.0f, 0.0f, 0.0f, 0.0f };

        static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();

        void SetBoneData(int boneID, float weight);
    };

    class IMesh
    {
    public:
        IMesh(bool assignID = true);
        virtual ~IMesh() = default;

        virtual void CreateVertexBuffers(Device* device) = 0;
        virtual void CreateIndexBuffers(Device* device) = 0;
        virtual void DestroyBuffers() = 0;

        virtual void Bind(VkCommandBuffer commandBuffer, VkBuffer instBuffer) = 0;
        virtual void Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex = 0) = 0;

        uint32_t GetID() const { return mMeshID; }

    public:
        // Buffers
        bool mHasIndexBuffer = false;
        uint32_t mTriangleCount = 0;
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount = 0;

        Buffer mVertexBuffer;
        Buffer mIndexBuffer;
        GLuint mVAO, mVBO, mEBO;

        // Mesh data
        std::vector<Vertex> mVertices{};
        std::vector<uint32_t> mIndices{};

        // Unique mesh index
        uint32_t mMeshID = 0;

        // Tex data if from memory
        std::vector<unsigned char> mAlbedoTextureData{};
        std::vector<unsigned char> mNormalTextureData{};
        std::vector<unsigned char> mMetalnessTextureData{};
        std::vector<unsigned char> mRoughnessTextureData{};
        std::vector<unsigned char> mOcclusionTextureData{};
        std::vector<unsigned char> mEmissiveTextureData{};
        uint32_t mAlbedoTextureSize = 0;
        uint32_t mNormalTextureSize = 0;
        uint32_t mMetalnessTextureSize = 0;
        uint32_t mRoughnessTextureSize = 0;
        uint32_t mOcclusionTextureSize = 0;
        uint32_t mEmissiveTextureSize = 0;

        // Otherwise path to textures
        bool loadedTextures = false;
        std::string albedoTexturePath{};
        std::string normalTexturePath{};
        std::string metalnessTexturePath{};
        std::string roughnessTexturePath{};
        std::string occlusionTexturePath{};
        std::string emissiveTexturePath{};
        uint32_t albedoTextureIndex = 10001;
        uint32_t normalTextureIndex = 10001;
        uint32_t metalnessTextureIndex = 10001;
        uint32_t roughnessTextureIndex = 10001;
        uint32_t occlusionTextureIndex = 10001;
        uint32_t emissiveTextureIndex = 10001;

        // Color 'factors'
        glm::vec4 baseColorFactor{ 1.f };
        float metallicFactor{ 0.f };
        float roughnessFactor{ 0.f };
        glm::vec4 emissiveFactor{ 0.f };

    private:
        static int GetTotalMeshCount() { return uniqueMeshIndex; }
        static int uniqueMeshIndex;
    };
}
