#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in ivec4 boneIds;
layout(location = 5) in vec4 weights;

layout(location = 0) flat out uint lightIndex;

layout(push_constant) uniform PushConstants {
    uint directionalLightCount;
    uint debugMode;
    vec2 invViewport;   // (1/width, 1/height) of your GBuffer/depth targets
} pc;

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
} uniforms;

struct Light {
    vec4 positionRadius;
    vec4 colorIntensity;
    vec4 directionInner;
    vec4 outerConeType;
};

#define MAX_LIGHTS 100000
layout(set = 0, binding = 6, std430) readonly buffer LightData {
    uint lightCount;
    Light lights[MAX_LIGHTS];
} lightData;

void main()
{
    // Compute and forward the light index to the fragment shader
    lightIndex = pc.directionalLightCount + gl_InstanceIndex;
    Light light = lightData.lights[lightIndex];

    vec3 lightPos = light.positionRadius.xyz;
    float range   = light.positionRadius.w;

    // Build model matrix: Translate(pos) * Scale(range)
    mat4 model = mat4(
        range, 0.0,   0.0,   0.0,
        0.0,   range, 0.0,   0.0,
        0.0,   0.0,   range, 0.0,
        lightPos.x, lightPos.y, lightPos.z, 1.0
    );

    gl_Position = uniforms.projectionView * model * vec4(position, 1.0);
}