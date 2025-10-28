#include <PCH/pch.h>
#include "DebugDrawResource.h"

#include "Graphics/Vulkan/Uniform/ShaderTypes.h"
#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/Model.h"

namespace Dog
{
    std::vector<DebugDrawResource::Line> DebugDrawResource::lines{};
    std::vector<DebugDrawResource::Rect> DebugDrawResource::rects{};
    std::vector<DebugDrawResource::Cube> DebugDrawResource::cubes{};
    std::vector<DebugDrawResource::Circle> DebugDrawResource::circles{};

    void DebugDrawResource::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
    {
        lines.emplace_back(start, end, color);
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

    void DebugDrawResource::Clear()
    {
        lines.clear();
        rects.clear();
        cubes.clear();
        circles.clear();
    }

    std::vector<InstanceUniforms> DebugDrawResource::GetInstanceData()
    {
        std::vector<InstanceUniforms> instanceData;
        instanceData.reserve(lines.size() + rects.size() + circles.size());

        for (const auto& line : lines)
        {
            InstanceUniforms& instance = instanceData.emplace_back();
            uint32_t textureIndex = 10001; // No texture

            // Calculate the line's properties: center, the vector from start to end, and its length.
            const glm::vec3 center = (line.start + line.end) * 0.5f;
            const glm::vec3 delta = line.end - line.start;
            const float length = glm::length(delta);

            const float thickness = 0.005f;
            instance.model = glm::mat4(1.0f);

            if (length > 1e-6f)
            {
                const glm::vec3 direction = delta / length;

                instance.model = glm::translate(instance.model, center);

                glm::quat rotation = glm::rotation(glm::vec3(1.0f, 0.0f, 0.0f), direction);
                instance.model *= glm::mat4_cast(rotation);

                instance.model = glm::scale(instance.model, glm::vec3(length * 0.5f, thickness * 0.5f, thickness * 0.5f));
            }
            else
            {
                instance.model = glm::translate(instance.model, line.start);
                instance.model = glm::scale(instance.model, glm::vec3(thickness * 0.5f));
            }

            instance.tint = line.color;
            instance.textureIndex = textureIndex;
        }

        for (const auto& rect : rects)
        {
            InstanceUniforms& instance = instanceData.emplace_back();
            uint32_t textureIndex = 10001; // No texture

            instance.model = glm::mat4(1.0f);
            instance.model = glm::translate(instance.model, rect.center);
            instance.model = glm::scale(instance.model, glm::vec3(rect.size.x * 0.5f, 0.02f, rect.size.y * 0.5f));
            instance.tint = rect.color;
            instance.textureIndex = textureIndex;
        }

        for (const auto& cube : cubes)
        {
            InstanceUniforms& instance = instanceData.emplace_back();
            uint32_t textureIndex = 10001; // No texture
            instance.model = glm::mat4(1.0f);
            instance.model = glm::translate(instance.model, cube.center);
            instance.model = glm::scale(instance.model, glm::vec3(cube.size * 0.5f));
            instance.tint = cube.color, 1.0f;
            instance.textureIndex = textureIndex;
        }

        for (const auto& circle : circles)
        {
            InstanceUniforms& instance = instanceData.emplace_back();
            uint32_t textureIndex = 10001; // Change to circle texture later

            instance.model = glm::mat4(1.0f);
            instance.model = glm::translate(instance.model, circle.center);
            instance.model = glm::scale(instance.model, glm::vec3(circle.radius * 0.5f, 0.02f, circle.radius * 0.5f));
            instance.tint = circle.color;
            instance.textureIndex = textureIndex;
        }

        return instanceData;
    }
}
