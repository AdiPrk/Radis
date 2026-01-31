/*****************************************************************//**
 * \file   GLMeshBuffer.h
 * \brief  OpenGL implementation of IMeshBuffer.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Graphics/RHI/IMeshBuffer.h"

namespace Radis
{
    class GLMeshBuffer : public IMeshBuffer
    {
    public:
        GLMeshBuffer() = default;
        ~GLMeshBuffer() override;

        void Upload(Device* device,
            const std::vector<Vertex>& vertices,
            const std::vector<uint32_t>& indices) override;
        void Destroy() override;

        void Bind(VkCommandBuffer cmd = nullptr) override;
        void Draw(VkCommandBuffer cmd = nullptr, uint32_t instanceBase = 0) override;

        // OpenGL-specific accessors
        GLuint GetVAO() const { return mVAO; }
        GLuint GetVBO() const { return mVBO; }
        GLuint GetEBO() const { return mEBO; }

    private:
        GLuint mVAO = 0;
        GLuint mVBO = 0;
        GLuint mEBO = 0;
    };
}