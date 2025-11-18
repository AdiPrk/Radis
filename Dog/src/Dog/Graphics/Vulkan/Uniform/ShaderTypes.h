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
        uint32_t indexOffset = 0;
        uint32_t vertexOffset = 0;
        uint32_t meshID = 777;

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
