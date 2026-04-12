/*****************************************************************//**
 * \file   ShaderTypes.h
 * \brief  Defines types used in shaders
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    struct CameraUniforms 
    {
        glm::mat4 projectionView;
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 inverseProjView;
        glm::vec3 cameraPos;
        int pad0 = 7777;
        glm::vec2 pixelJitter;
        int frameCount;
        int accumulationCount;
    };

    struct InstanceUniforms 
    {
        glm::mat4 model;
        glm::vec4 tint;
        glm::uvec4 textureIndices;
        glm::uvec4 textureIndices2; // zw are padding
        glm::vec4 baseColorFactor;
        glm::vec4 metallicRoughnessFactor; // zw are padding
        glm::vec4 emissiveFactor; // w is boneOffset
        uint32_t boneOffset = 10001;
        uint32_t indexOffset = 0;
        uint32_t vertexOffset = 0;
        uint32_t meshID = 777;

        const static uint32_t MAX_INSTANCES = 1000000;
    };

    struct SimpleInstanceUniforms
    {
        glm::mat4 model;
        
        const static uint32_t MAX_INSTANCES = 10000;
    };

    struct AnimationUniforms
    {
        VQS boneVQS;

        const static uint32_t MAX_BONES = 10000;
    };

    struct LightUniform
    {
        glm::vec4 positionRadius;    // xyz = position, w = radius
        glm::vec4 colorIntensity;    // xyz = color, w = intensity
        glm::vec4 directionInner;    // xyz = direction, w = innerCone
        glm::vec4 outerConeType;     // x = outerCone, y = type (0=dir, 1=point, 2=spot), zw = padding

        static const uint32_t MAX_LIGHTS = 100000;
    };

    // Push constants for light volume debug/control (16 bytes)
    struct LightVolumePushConstants
    {
        uint32_t directionalLightCount; // Offset into light SSBO where local lights begin
        uint32_t debugMode;             // 0=normal, 1=volume tint, 2=density heatmap
        glm::vec2 invView;              // (1/width, 1/height) of GBuffer/depth targets
    };

    struct MeshDataUniform
    {
        uint32_t color;
        uint32_t normal;
        float texU, texV;
    };

    static uint32_t PackColor(const glm::vec3& c)
    {
        return (uint32_t(glm::clamp(c.r, 0.f, 1.f) * 255.f))
            | (uint32_t(glm::clamp(c.g, 0.f, 1.f) * 255.f) << 8)
            | (uint32_t(glm::clamp(c.b, 0.f, 1.f) * 255.f) << 16);
    }

    static uint32_t PackNormal(glm::vec3 n)
    {
        n /= std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
        if (n.z < 0.f) {
            float x = n.x, y = n.y;
            n.x = (1.f - std::abs(y)) * (x >= 0.f ? 1.f : -1.f);
            n.y = (1.f - std::abs(x)) * (y >= 0.f ? 1.f : -1.f);
        }
        auto ex = int16_t(glm::clamp(n.x, -1.f, 1.f) * 32767.f);
        auto ey = int16_t(glm::clamp(n.y, -1.f, 1.f) * 32767.f);
        return uint32_t(uint16_t(ex)) | (uint32_t(uint16_t(ey)) << 16);
    }
}
