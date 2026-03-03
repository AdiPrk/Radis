#version 460

// Match deferred.vert inputs
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;    // unused
layout(location = 2) in vec3 normal;   // unused
layout(location = 3) in vec2 texCoord; // unused
layout(location = 4) in ivec4 boneIds;
layout(location = 5) in vec4 weights;

layout(location = 0) out float vLightViewZ;

const uint INVALID_TEXTURE_INDEX = 10001;
const int  INVALID_BONE_ID       = -1;

struct VQS {
    vec4 rotation;
    vec3 translation;
    vec3 scale;
};

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b, std140)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b, std430)
    #define INSTANCE_ID gl_InstanceIndex
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
    #define INSTANCE_ID gl_InstanceID + gl_BaseInstance
#endif

// Shadow camera UBO (light matrices + depth remap params)
UBO_LAYOUT(0, 0) uniform ShadowUniforms
{
    mat4 lightViewProj;
    mat4 lightView;
    float z0;
    float z1;
    float pad0;
    float pad1;
} sh;

struct Instance
{
    mat4 model;
    vec4 tint;
    uvec4 textureIndices;
    uvec4 textureIndices2;
    vec4 baseColorFactor;
    vec4 metallicRoughnessFactor;
    vec4 emissiveFactor;
    uint boneOffset;
    uint indexOffset;
    uint vertexOffset;
};

SSBO_LAYOUT(0, 1) readonly buffer InstanceData
{
    Instance instances[];
};

SSBO_LAYOUT(0, 2) readonly buffer BoneBuffer
{
    VQS finalBoneVQS[];
} animationData;

// Rotate vector by a quat
vec3 rotate(vec4 q, vec3 v) {
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

void main()
{
    vec4 totalPosition = vec4(0.0);
    Instance instance = instances[INSTANCE_ID];

    bool validBoneFound = false;
    if (instance.boneOffset != INVALID_TEXTURE_INDEX)
    {
        for (int i = 0; i < 4; i++)
        {
            if (boneIds[i] == INVALID_BONE_ID) continue;

            VQS transform = animationData.finalBoneVQS[instance.boneOffset + boneIds[i]];

            vec3 localPosition = rotate(transform.rotation, position * transform.scale) + transform.translation;
            totalPosition += vec4(localPosition, 1.0) * weights[i];
            validBoneFound = true;
        }
    }

    if (!validBoneFound)
        totalPosition = vec4(position, 1.0);

    vec4 worldPos = instance.model * vec4(totalPosition.xyz, 1.0);

    // Light view depth (positive forward)
    vec4 lv = sh.lightView * worldPos;
    vLightViewZ = -lv.z;

    gl_Position = sh.lightViewProj * worldPos;
}
