#include <PCH/pch.h>
#include "GLMesh.h"
#include "../Vulkan/Core/Buffer.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"
#include "GLShader.h"

namespace Dog
{
    GLMesh::GLMesh(bool assignID)
        : IMesh(assignID)
    {
    }

    // device will be nullptr
    void GLMesh::CreateVertexBuffers(Device* device)
    {
        mVertexCount = static_cast<uint32_t>(mVertices.size());
        mTriangleCount = mVertexCount / 3;
        mHasIndexBuffer = !mIndices.empty();

        // Generate and bind VAO/VBO
        glGenVertexArrays(1, &mVAO);
        glGenBuffers(1, &mVBO);

        glBindVertexArray(mVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(Vertex), mVertices.data(), GL_STATIC_DRAW);

        // ---- Vertex layout ----
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

        glBindVertexArray(0);
    }

    void GLMesh::CreateIndexBuffers(Device* device)
    {
        if (mIndices.empty())
            return;

        glBindVertexArray(mVAO);

        glGenBuffers(1, &mEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(uint32_t), mIndices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    void GLMesh::Cleanup()
    {
        if (mEBO) {
            glDeleteBuffers(1, &mEBO);
            mEBO = 0;
        }
        if (mVBO) {
            glDeleteBuffers(1, &mVBO);
            mVBO = 0;
        }
        if (mVAO) {
            glDeleteVertexArrays(1, &mVAO);
            mVAO = 0;
        }
    }

    // all will be nullptr
    void GLMesh::Bind(VkCommandBuffer commandBuffer, VkBuffer instBuffer)
    {
        glBindVertexArray(mVAO);
    }

    void GLMesh::Draw(VkCommandBuffer commandBuffer, uint32_t baseIndex)
    {
        glBindVertexArray(mVAO);
        glDrawElementsInstancedBaseInstance(
            GL_TRIANGLES,
            static_cast<GLsizei>(mIndices.size()),
            GL_UNSIGNED_INT,
            nullptr,
            1,
            baseIndex
        );
        glBindVertexArray(0);
    }
}
