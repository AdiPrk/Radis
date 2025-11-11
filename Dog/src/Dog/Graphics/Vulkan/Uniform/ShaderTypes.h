#pragma once

namespace Dog
{
    struct CameraUniforms 
    {
        glm::mat4 projectionView;
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec3 cameraPos;
    };

    struct InstanceUniforms 
    {
        glm::mat4 model;
        glm::vec4 tint;
        glm::uvec4 textureIndicies;
        glm::uvec4 textureIndicies2; // zw are padding
        glm::vec4 baseColorFactor;
        glm::vec4 metallicRoughnessFactor; // zw are padding
        glm::vec4 emissiveFactor; // w is boneOffset
        uint32_t boneOffset = 10001;
        glm::vec3 _padding;

        const static uint32_t MAX_INSTANCES = 10000;
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

    struct LightUniform {
        glm::vec3 position;
        float radius;        // For point/spot attenuation
        glm::vec3 color;
        float intensity;
        glm::vec3 direction;
        float innerCone;     // for spot
        float outerCone;     // for spot
        int type;            // 0=dir, 1=point, 2=spot
        uint32_t _padding[2]; 
    };
}
