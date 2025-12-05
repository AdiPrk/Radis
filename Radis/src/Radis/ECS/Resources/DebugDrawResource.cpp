#include <PCH/pch.h>
#include "DebugDrawResource.h"

#include "Graphics/Vulkan/Uniform/ShaderTypes.h"
#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/Common/Model.h"

namespace Dog
{
    std::vector<DebugDrawResource::Line> DebugDrawResource::lines{};
    std::vector<DebugDrawResource::Rect> DebugDrawResource::rects{};
    std::vector<DebugDrawResource::Cube> DebugDrawResource::cubes{};
    std::vector<DebugDrawResource::Circle> DebugDrawResource::circles{};

    void DebugDrawResource::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float thickness)
    {
        lines.emplace_back(start, end, color, thickness);
    }

    void DebugDrawResource::DrawRect(const glm::vec3& center, const glm::vec2& size, const glm::vec4& color)
    {
        rects.emplace_back(center, size, color);
    }

    void DebugDrawResource::DrawCube(const glm::vec3& center, const glm::vec3& size, const glm::vec4& color)
    {
        cubes.emplace_back(center, size, color);
    }

    void DebugDrawResource::DrawCircle(const glm::vec3& center, float radius, const glm::vec4& color)
    {
        circles.emplace_back(center, radius, color); 
    }

    std::vector<InstanceUniforms> DebugDrawResource::CreateDebugLightTest()
    {
        uint32_t gridSize = 7;
        std::vector<InstanceUniforms> instanceData;
        instanceData.reserve(gridSize * gridSize);

        float spacing = 1.0f;

        for (uint32_t y = 0; y < gridSize; ++y)
        {
            for (uint32_t x = 0; x < gridSize; ++x)
            {
                InstanceUniforms& instance = instanceData.emplace_back();
                instance.model = glm::mat4(1.0f);
                instance.model = glm::translate(instance.model, glm::vec3(
                    (x - gridSize / 2.0f) * spacing,
                    (y - gridSize / 2.0f) * spacing,
                    0.0f
                ));
                instance.model = glm::scale(instance.model, glm::vec3(0.2f));

                instance.tint = glm::vec4(1.0f, 0.f, 0.f, 1.f);
                instance.textureIndicies = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
                instance.textureIndicies2 = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);

                // Roughness: left ¨ right (X)
                // Metalness: bottom ¨ top (Y)
                float roughness = static_cast<float>(x) / static_cast<float>(gridSize - 1);
                float metalness = static_cast<float>(y) / static_cast<float>(gridSize - 1);

                instance.metallicRoughnessFactor = glm::vec4(metalness, roughness, 0.0f, 0.0f);
                instance.baseColorFactor = glm::vec4(1.0f);
            }
        }


        return instanceData;
    }

    void DebugDrawResource::Clear()
    {
        lines.clear();
        rects.clear();
        cubes.clear();
        circles.clear();
    }

    /*********************************************************************
    * param:  gridSize: Number of lines in each direction from the center (total lines = gridSize * 2 + 1)
    * param:  step: Distance between each grid line
    *
    * brief:  Draws a standard editor grid on the XZ plane (Y=0).
    *********************************************************************/
    void DebugDrawResource::DrawEditorGrid(int gridSize, float step)
    {
        // Define colors
        glm::vec4 gridColor(0.4f, 0.4f, 0.4f, 0.75f);   // A medium grey
        glm::vec4 xAxisColor(1.0f, 0.0f, 0.0f, 0.75f);  // Red
        glm::vec4 zAxisColor(0.0f, 0.0f, 1.0f, 0.75f);  // Blue

        float fGridSize = (float)gridSize * step;

        // Draw lines parallel to the Z-axis (varying x)
        for (int i = -gridSize; i <= gridSize; ++i)
        {
            float x = (float)i * step;
            glm::vec3 start(x, 0.0f, -fGridSize);
            glm::vec3 end(x, 0.0f, fGridSize);

            // Use Z-axis color if x is 0, otherwise use regular grid color
            glm::vec4 color = (i == 0) ? zAxisColor : gridColor;

            DebugDrawResource::DrawLine(start, end, color);
        }

        // Draw lines parallel to the X-axis (varying z)
        for (int i = -gridSize; i <= gridSize; ++i)
        {
            float z = (float)i * step;
            glm::vec3 start(-fGridSize, 0.0f, z);
            glm::vec3 end(fGridSize, 0.0f, z);

            // Use X-axis color if z is 0, otherwise use regular grid color
            glm::vec4 color = (i == 0) ? xAxisColor : gridColor;

            DebugDrawResource::DrawLine(start, end, color);
        }
    }

    std::vector<InstanceUniforms> DebugDrawResource::GetInstanceData()
    {
        std::vector<InstanceUniforms> instanceData;
        instanceData.reserve(lines.size() + rects.size() + circles.size());

        // Define a robust "up" vector for calculating orientation
        const glm::vec3 UP_VECTOR(0.0f, 1.0f, 0.0f);
        const glm::vec3 ALT_UP_VECTOR(1.0f, 0.0f, 0.0f);

        for (const auto& line : lines)
        {
            InstanceUniforms& instance = instanceData.emplace_back();
            const float thickness = line.thickness;

            const glm::vec3 center = (line.start + line.end) * 0.5f;
            const glm::vec3 delta = line.end - line.start;

            // Use squared length for the check. This avoids a sqrt operation
            // for the zero-length 'else' case.
            const float lengthSq = glm::dot(delta, delta);

            // Epsilon check (1e-6 * 1e-6 = 1e-12)
            if (lengthSq > 1e-12f)
            {
                // Now we calculate the actual length and direction
                const float length = glm::sqrt(lengthSq);
                const glm::vec3 direction = delta / length; // This is our new X-axis

                // --- Build the rotation matrix basis vectors ---
                // Find a vector that is not collinear with 'direction'
                glm::vec3 up = UP_VECTOR;
                if (glm::abs(glm::dot(direction, up)) > 0.999f)
                {
                    up = ALT_UP_VECTOR; // Use alternate if parallel
                }

                // Use cross products to find the other two orthogonal axes
                const glm::vec3 newZ = glm::normalize(glm::cross(direction, up)); // New Z-axis
                const glm::vec3 newY = glm::cross(newZ, direction); // New Y-axis

                // --- Directly construct the final Model Matrix (T * R * S) ---
                // The final matrix columns are the scaled basis vectors (R*S)
                // and the translation vector (T).

                const float halfLength = length * 0.5f;
                const float halfThick = thickness * 0.5f;

                instance.model = glm::mat4(
                    // Column 1: New X-axis (direction) scaled by length
                    glm::vec4(direction * halfLength, 0.0f),
                    // Column 2: New Y-axis scaled by thickness
                    glm::vec4(newY * halfThick, 0.0f),
                    // Column 3: New Z-axis scaled by thickness
                    glm::vec4(newZ * halfThick, 0.0f),
                    // Column 4: Translation (center)
                    glm::vec4(center, 1.0f)
                );
            }
            else // This is a zero-length line (a "dot")
            {
                // Directly construct a uniform scale (T * S) matrix
                const float scale = thickness * 0.5f;
                instance.model = glm::mat4(
                    // Column 1
                    scale, 0.0f, 0.0f, 0.0f,
                    // Column 2
                    0.0f, scale, 0.0f, 0.0f,
                    // Column 3
                    0.0f, 0.0f, scale, 0.0f,
                    // Column 4 (Translation)
                    line.start.x, line.start.y, line.start.z, 1.0f
                );
            }

            instance.tint = line.color;
            instance.textureIndicies = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.textureIndicies2 = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.baseColorFactor = glm::vec4(1.0f);
        }

        for (const auto& rect : rects)
        {
            InstanceUniforms& instance = instanceData.emplace_back();

            instance.model = glm::mat4(1.0f);
            instance.model = glm::translate(instance.model, rect.center);
            instance.model = glm::scale(instance.model, glm::vec3(rect.size.x * 0.5f, 0.02f, rect.size.y * 0.5f));
            instance.tint = rect.color;
            instance.textureIndicies = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.textureIndicies2 = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.baseColorFactor = glm::vec4(1.0f);
        }

        for (const auto& cube : cubes)
        {
            InstanceUniforms& instance = instanceData.emplace_back();
            instance.model = glm::mat4(1.0f);
            instance.model = glm::translate(instance.model, cube.center);
            instance.model = glm::scale(instance.model, glm::vec3(cube.size * 0.5f));
            instance.tint = cube.color, 1.0f;
            instance.textureIndicies = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.textureIndicies2 = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.baseColorFactor = glm::vec4(1.0f);
        }

        for (const auto& circle : circles)
        {
            InstanceUniforms& instance = instanceData.emplace_back();
            uint32_t textureIndex = TextureLibrary::INVALID_TEXTURE_INDEX; // Change to circle texture later

            instance.model = glm::mat4(1.0f);
            instance.model = glm::translate(instance.model, circle.center);
            instance.model = glm::scale(instance.model, glm::vec3(circle.radius * 0.5f, 0.02f, circle.radius * 0.5f));
            instance.tint = circle.color;
            instance.textureIndicies = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.textureIndicies2 = glm::uvec4(TextureLibrary::INVALID_TEXTURE_INDEX);
            instance.baseColorFactor = glm::vec4(1.0f);
        }

        return instanceData;
    }

    uint32_t DebugDrawResource::GetInstanceDataSize()
    {
        uint32_t totalSize = static_cast<uint32_t>(lines.size() + rects.size() + cubes.size() + circles.size());
        return totalSize;
    }
}
