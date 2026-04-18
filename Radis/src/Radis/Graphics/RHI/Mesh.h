/*****************************************************************//**
 * \file   Mesh.h
 * \brief  CPU-side mesh class with abstracted GPU buffer management.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "IMeshBuffer.h"

namespace Radis
{
    class Device;

    /**
     * \brief CPU-side mesh representation with optional GPU buffer.
     * 
     * This class holds all mesh data (vertices, indices, material info) on the CPU
     * and delegates GPU operations to an IMeshBuffer implementation.
     */
    class Mesh
    {
    public:
        Mesh(bool assignID = true);
        ~Mesh();

        // Non-copyable, movable
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;

        // GPU buffer management
        void UploadToGPU(Device* device);
        void ReleaseGPU();
        void RecreateBuffer(Device* device);

        // Rendering
        void Bind(VkCommandBuffer cmd = nullptr);
        void Draw(VkCommandBuffer cmd = nullptr, uint32_t instanceBase = 0);

        // Accessors
        uint32_t GetID() const { return mMeshID; }
        IMeshBuffer* GetBuffer() const { return mBuffer.get(); }
        bool HasGPUBuffer() const { return mBuffer != nullptr && mBuffer->IsUploaded(); }

        // Buffer state (forwarded from IMeshBuffer or computed from CPU data)
        uint32_t GetVertexCount() const { return static_cast<uint32_t>(mVertices.size()); }
        uint32_t GetIndexCount() const { return static_cast<uint32_t>(mIndices.size()); }
        uint32_t GetTriangleCount() const { return GetIndexCount() / 3; }
        bool HasIndexBuffer() const { return !mIndices.empty(); }

    public:       
        // Geometry
        std::vector<Vertex> mVertices{};
        std::vector<uint32_t> mIndices{};

        // Unique mesh index
        uint32_t mMeshID = 0;

        // Embedded texture data (cleared after upload to TextureLibrary)
        std::vector<unsigned char> mAlbedoTextureData{};
        std::vector<unsigned char> mNormalTextureData{};
        std::vector<unsigned char> mMetalnessTextureData{};
        std::vector<unsigned char> mRoughnessTextureData{};
        std::vector<unsigned char> mOcclusionTextureData{};
        std::vector<unsigned char> mEmissiveTextureData{};
        std::vector<unsigned char> mTransmissionTextureData{};
        uint32_t mAlbedoTextureSize = 0;
        uint32_t mNormalTextureSize = 0;
        uint32_t mMetalnessTextureSize = 0;
        uint32_t mRoughnessTextureSize = 0;
        uint32_t mOcclusionTextureSize = 0;
        uint32_t mEmissiveTextureSize = 0;
        uint32_t mTransmissionTextureSize = 0;

        // Texture paths (alternative to embedded data)
        bool loadedTextures = false;
        std::string albedoTexturePath{};
        std::string normalTexturePath{};
        std::string metalnessTexturePath{};
        std::string roughnessTexturePath{};
        std::string occlusionTexturePath{};
        std::string emissiveTexturePath{};
        std::string transmissionTexturePath{};

        // Texture indices (set after textures loaded into TextureLibrary)
        uint32_t albedoTextureIndex = UINT32_MAX;
        uint32_t normalTextureIndex = UINT32_MAX;
        uint32_t metalnessTextureIndex = UINT32_MAX;
        uint32_t roughnessTextureIndex = UINT32_MAX;
        uint32_t occlusionTextureIndex = UINT32_MAX;
        uint32_t emissiveTextureIndex = UINT32_MAX;
        uint32_t transmissionTextureIndex = UINT32_MAX;

        bool mMetallicRoughnessCombined = false;

        // Material factors
        glm::vec4 baseColorFactor{ 1.f };
        float metallicFactor{ 0.f };
        float roughnessFactor{ 0.f };
        glm::vec4 emissiveFactor{ 0.f };
        float transmissionFactor{ 0.f };
        float ior{ 1.5f }; // glass default

    private:
        // GPU Mesh Buffer
        std::unique_ptr<IMeshBuffer> mBuffer;

        static int GetTotalMeshCount() { return uniqueMeshIndex; }
        static int uniqueMeshIndex;
    };
}