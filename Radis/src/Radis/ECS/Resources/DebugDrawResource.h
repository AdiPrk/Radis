#pragma once

#include "IResource.h"

namespace Radis
{

    struct InstanceUniforms;

    struct DebugDrawResource : public IResource
    {
        DebugDrawResource() {}

        static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color = glm::vec4(1), float thickness = 0.005f);
        static void DrawRect(const glm::vec3& center, const glm::vec2& size, const glm::vec4& color = glm::vec4(1));
        static void DrawCube(const glm::vec3& center, const glm::vec3& size, const glm::vec4& color = glm::vec4(1));
        static void DrawCircle(const glm::vec3& center, float radius, const glm::vec4& color = glm::vec4(1));

        static std::vector<InstanceUniforms> CreateDebugLightTest();
        static std::vector<InstanceUniforms> GetInstanceData();
        static uint32_t GetInstanceDataSize();
        static void Clear();

        /*********************************************************************
         * param:  gridSize: Number of lines in each direction from the center (total lines = gridSize * 2 + 1)
         * param:  step: Distance between each grid line
         *
         * brief:  Draws a standard editor grid on the XZ plane (Y=0).
         *********************************************************************/
        static void DrawEditorGrid(int gridSize = 50, float step = 1.0f);

    private:
        struct Line
        {
            glm::vec3 start;
            glm::vec3 end;
            glm::vec4 color;
            float thickness;
        };
        static std::vector<Line> lines;

        struct Rect
        {
            glm::vec3 center;
            glm::vec2 size;
            glm::vec4 color;
        };
        static std::vector<Rect> rects;

        struct Cube
        {
            glm::vec3 center;
            glm::vec3 size;
            glm::vec4 color;
        };
        static std::vector<Cube> cubes;

        struct Circle
        {
            glm::vec3 center;
            float radius;
            glm::vec4 color;
        };
        static std::vector<Circle> circles;
    };
}
