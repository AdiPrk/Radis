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

        static const uint32_t MAX_LIGHTS = 1000;
    };

    /*struct MeshDataUniform {
        glm::vec3 position; float _padding1 = 777.0f;
        glm::vec3 color;    float _padding2 = 777.0f;
        glm::vec3 normal;   float _padding3 = 777.0f;
        glm::vec2 texCoord; glm::vec2 _padding4 = glm::vec2(777.0f);
    };*/

    /*struct MeshDataUniform2 {
        glm::vec4 positionColorR;
        glm::vec4 colorGBnormalXY;
        glm::vec3 normalZtexXY; uint32_t padding;
    };*/

    struct MeshDataUniform
    {
        float posX, posY, posZ;
        float colorR, colorG, colorB;
        float normalX, normalY, normalZ;
        float texU, texV; float _padding = 777.0f;
    };
}
