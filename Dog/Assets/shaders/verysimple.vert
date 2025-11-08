#version 460

// Per Vertex Inputs
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in ivec4 boneIds;
layout(location = 5) in vec4 weights;

// Outputs -----------------------------------------
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec4 fragTint;
layout(location = 2) out vec3 fragWorldNormal;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) flat out uvec4 textureIndex;
layout(location = 5) flat out uvec4 textureIndex2;
layout(location = 6) flat out vec4 baseColorFactor;
layout(location = 7) flat out vec4 metallicRoughnessFactor;
layout(location = 8) flat out vec4 emissiveFactor;
layout(location = 9) flat out uint instanceIndex;
layout(location = 10) out vec3 fragWorldPos;
// -------------------------------------------------

const float PI = 3.14159265359;
const uint INVALID_TEXTURE_INDEX = 10001;

struct VQS {
    vec4 rotation;    // Quat
    vec3 translation; // Vector
    vec3 scale;       // Scale
};

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b, std140)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b, std430)
    #define INSTANCE_ID gl_InstanceIndex
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
    #define INSTANCE_ID gl_BaseInstance
#endif

UBO_LAYOUT(0, 0) uniform Uniforms
{
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    vec3 cameraPos;
    vec3 lightDir;
    vec4 lightColor; // w = intensity
} uniforms;

struct Instance
{
    mat4 model;
    vec4 tint;
    uvec4 textureIndicies;
    uvec4 textureIndicies2;
    vec4 baseColorFactor;
    vec4 metallicRoughnessFactor;
    vec4 emissiveFactor;
    uint boneOffset;
};

SSBO_LAYOUT(0, 1) readonly buffer InstanceData
{
    Instance instances[10000];
};

SSBO_LAYOUT(0, 2) readonly buffer BoneBuffer
{
    VQS finalBoneVQS[10000];
} animationData;

// Helper Functions --------------------------------
// Rotate vector by a quat
vec3 rotate(vec4 q, vec3 v) {
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}
// ----------------------------------------------------

void main() 
{
    vec4 totalPosition = vec4(0.0f);
    vec3 totalNormal = vec3(0.0f);
    
    Instance instance = instances[INSTANCE_ID];

    bool validBoneFound = false;
    if (instance.boneOffset != 10001)
    {
        for (uint i = 0; i < 4 ; i++)
        {
            if(boneIds[i] == -1) continue;
        
            VQS transform = animationData.finalBoneVQS[instance.boneOffset + boneIds[i]];
        
            // --- Position Transformation ---
            vec3 localPosition = rotate(transform.rotation, position * transform.scale) + transform.translation;
        
            // --- Normal Transformation
            vec3 inverseScale = vec3(1.0) / transform.scale;
            vec3 localNormal = rotate(transform.rotation, normal * inverseScale);
        
            // --- Accumulate weighted results ---
            totalPosition += vec4(localPosition, 1.0f) * weights[i];
            totalNormal += localNormal * weights[i];
        
            validBoneFound = true;
        }

    }
    
    if (!validBoneFound || totalPosition == vec4(0.0))
    {
		totalPosition = vec4(position, 1.0);
        totalNormal = normal;
	}

    vec4 worldPos = instance.model * vec4(totalPosition.xyz, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(instance.model)));
    vec3 worldNormal = normalize(normalMatrix * normalize(totalNormal));

    gl_Position = uniforms.projectionView * worldPos;

    fragColor = color;
    fragTint = instance.tint;
    fragTexCoord = texCoord;
    textureIndex = instance.textureIndicies;
    textureIndex2 = instance.textureIndicies2;
    baseColorFactor = instance.baseColorFactor;
    metallicRoughnessFactor = instance.metallicRoughnessFactor;
    emissiveFactor = instance.emissiveFactor;
    instanceIndex = INSTANCE_ID;
    fragWorldPos = worldPos.xyz;
    fragWorldNormal = worldNormal;
}