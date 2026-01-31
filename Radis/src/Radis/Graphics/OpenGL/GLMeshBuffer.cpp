/*****************************************************************//**
 * \file   GLMeshBuffer.cpp
 * \brief  OpenGL implementation of IMeshBuffer.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "GLMeshBuffer.h"

namespace Radis
{
    GLMeshBuffer::~GLMeshBuffer()
    {
        Destroy();
    }

    void GLMeshBuffer::Upload(Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        if (vertices.empty())
        {
            RADIS_WARN("GLMeshBuffer::Upload called with empty vertices!");
            return;
        }

        // Destroy existing buffers if any
        Destroy();

        mVertexCount = static_cast<uint32_t>(vertices.size());
        mIndexCount = static_cast<uint32_t>(indices.size());
        mHasIndexBuffer = !indices.empty();

        // Generate and bind VAO/VBO
        glGenVertexArrays(1, &mVAO);
        glGenBuffers(1, &mVBO);

        glBindVertexArray(mVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        // Vertex layout
        // location 0: position (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        // location 1: color (vec3)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

        // location 2: normal (vec3)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        // location 3: uv (vec2)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

        // location 4: bone IDs (ivec4)
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, boneIDs));

        // location 5: weights (vec4)
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, weights));

        // Create index buffer if needed
        if (mHasIndexBuffer)
        {
            glGenBuffers(1, &mEBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
        }

        glBindVertexArray(0);

        mIsUploaded = true;
    }

    void GLMeshBuffer::Destroy()
    {
        if (mEBO)
        {
            glDeleteBuffers(1, &mEBO);
            mEBO = 0;
        }
        if (mVBO)
        {
            glDeleteBuffers(1, &mVBO);
            mVBO = 0;
        }
        if (mVAO)
        {
            glDeleteVertexArrays(1, &mVAO);
            mVAO = 0;
        }

        mVertexCount = 0;
        mIndexCount = 0;
        mHasIndexBuffer = false;
        mIsUploaded = false;
    }

    void GLMeshBuffer::Bind(VkCommandBuffer cmd)
    {
        glBindVertexArray(mVAO);
    }

    void GLMeshBuffer::Draw(VkCommandBuffer cmd, uint32_t instanceBase)
    {
        glBindVertexArray(mVAO);
        glDrawElementsInstancedBaseInstance(
            GL_TRIANGLES,
            static_cast<GLsizei>(mIndexCount),
            GL_UNSIGNED_INT,
            nullptr,
            1,
            instanceBase
        );
        glBindVertexArray(0);
    }
}