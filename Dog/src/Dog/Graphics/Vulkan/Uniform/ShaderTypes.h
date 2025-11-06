#pragma once

namespace Dog
{
    struct CameraUniforms 
    {
        glm::mat4 projectionView;
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec3 cameraPos; uint32_t padding1;
        glm::vec3 lightDir; uint32_t padding2;
        glm::vec3 lightColor; uint32_t padding3;
        float lightIntensity;
    };

    struct InstanceUniforms 
    {
        glm::mat4 model;
        glm::vec4 tint;
        uint32_t textureIndex;
        uint32_t boneOffset = 10001;

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
}
