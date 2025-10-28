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
        glBufferData(GL_ARRAY_BUFFER, mSimpleVertices.size() * sizeof(SimpleVertex), mSimpleVertices.data(), GL_STATIC_DRAW);

        // ---- Vertex layout ----
        // location 0: position (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, position));

        // location 1: color (vec3)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, color));

        // location 2: normal (vec3)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, normal));

        // location 3: uv (vec2)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, uv));

        GLShader::SetupInstanceVBO();
        glBindBuffer(GL_ARRAY_BUFFER, GLShader::GetInstanceVBO());

        size_t vec4Size = sizeof(glm::vec4);
        for (int i = 0; i < 4; i++)
        {
            glEnableVertexAttribArray(4 + i);
            glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(vec4Size * i));
            glVertexAttribDivisor(4 + i, 1);
        }

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

        if (!mIndices.empty())
        {
            //glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mIndices.size()), GL_UNSIGNED_INT, nullptr);
            glDrawElementsInstancedBaseInstance(
                GL_TRIANGLES,
                static_cast<GLsizei>(mIndices.size()),
                GL_UNSIGNED_INT,
                nullptr,
                1,
                baseIndex
            );
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mVertices.size()));
        }

        glBindVertexArray(0);
    }
}
