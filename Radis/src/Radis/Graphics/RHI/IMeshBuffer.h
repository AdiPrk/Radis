/*****************************************************************//**
 * \file   IMeshBuffer.h
 * \brief  Abstract GPU buffer interface for mesh rendering.
 *         Separates CPU mesh data from GPU-specific buffer implementations.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Graphics/Vulkan/Core/Buffer.h"

namespace Radis
{
    class Device;

    struct Vertex
    {
        glm::vec3 position{ 0.f };
        glm::vec3 color{ 1.f };
        glm::vec3 normal{ 0.f };
        glm::vec2 uv{ 0.f };

        static constexpr int MAX_BONE_INFLUENCE = 4;

        std::array<int, MAX_BONE_INFLUENCE> boneIDs = { -1, -1, -1, -1 };
        std::array<float, MAX_BONE_INFLUENCE> weights = { 0.0f, 0.0f, 0.0f, 0.0f };

        static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();

        void SetBoneData(int boneID, float weight);
    };

    /**
     * \brief Abstract GPU buffer - handles upload, bind, draw operations.
     *
     * This interface abstracts away the graphics API-specific buffer management,
     * allowing the Mesh class to remain API-agnostic.
     */
    class IMeshBuffer
    {
    public:
        virtual ~IMeshBuffer() = default;

        /**
         * \brief Upload vertex and index data to the GPU.
         * \param device The graphics device (may be nullptr for OpenGL)
         * \param vertices The vertex data to upload
         * \param indices The index data to upload
         */
        virtual void Upload(Device* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) = 0;

        /**
         * \brief Destroy GPU resources.
         */
        virtual void Destroy() = 0;

        /**
         * \brief Bind the mesh buffers for rendering.
         * \param cmd Vulkan command buffer (nullptr for OpenGL)
         */
        virtual void Bind(VkCommandBuffer cmd = nullptr) = 0;

        /**
         * \brief Draw the mesh.
         * \param cmd Vulkan command buffer (nullptr for OpenGL)
         * \param instanceBase Base instance for instanced rendering
         */
        virtual void Draw(VkCommandBuffer cmd = nullptr, uint32_t instanceBase = 0) = 0;

        // Vulkan-specific accessors for raytracing
        virtual Buffer& GetVertexBuffer() { static Buffer dummy{}; return dummy; }
        virtual Buffer& GetIndexBuffer() { static Buffer dummy{}; return dummy; }
        virtual const Buffer& GetVertexBuffer() const { static Buffer dummy{}; return dummy; }
        virtual const Buffer& GetIndexBuffer() const { static Buffer dummy{}; return dummy; }

        // Buffer state accessors
        uint32_t GetVertexCount() const { return mVertexCount; }
        uint32_t GetIndexCount() const { return mIndexCount; }
        uint32_t GetTriangleCount() const { return mIndexCount / 3; }
        bool HasIndexBuffer() const { return mHasIndexBuffer; }
        bool IsUploaded() const { return mIsUploaded; }

    protected:
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount = 0;
        bool mHasIndexBuffer = false;
        bool mIsUploaded = false;
    };

    /**
     * \brief Factory function to create the appropriate mesh buffer for the current graphics API.
     * \return A unique_ptr to the created mesh buffer
     */
    std::unique_ptr<IMeshBuffer> CreateMeshBuffer();
}