#version 460

// Per Vertex Inputs
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in ivec4 boneIds;
layout(location = 5) in vec4 weights;
layout(location = 6) in vec4 tangent;   // xyz + bitangent sign in w; zero for meshes without normal maps

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
layout(location = 11) out vec4 fragWorldTangent;
// -------------------------------------------------

const float PI = 3.14159265359;
const uint INVALID_TEXTURE_INDEX = 0xFFFFFFFFu;
const int INVALID_BONE_ID = -1;

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b, std140)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b, std430)
    #define INSTANCE_ID gl_InstanceIndex
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
    #define INSTANCE_ID gl_InstanceID + gl_BaseInstance
#endif

UBO_LAYOUT(0, 0) uniform Uniforms
{
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
} uniforms;

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

// Each column holds a row of an affine transform, so a point skins as vec4(point, 1.0) * matrix.
SSBO_LAYOUT(0, 2) readonly buffer SkinBuffer
{
    mat3x4 skinMatrices[];
} animationData;

void main() 
{
    vec4 totalPosition = vec4(0.0f);
    vec3 totalNormal = vec3(0.0f);
    vec3 totalTangent = vec3(0.0f);
    
    Instance instance = instances[INSTANCE_ID];

    bool validBoneFound = false;
    if (instance.boneOffset != INVALID_TEXTURE_INDEX)
    {
        for (int i = 0; i < 4 ; i++)
        {
            if(boneIds[i] == INVALID_BONE_ID) continue;
            mat3x4 skin = animationData.skinMatrices[instance.boneOffset + boneIds[i]];

            // Normals use the same matrix, which is exact unless a joint is scaled non-uniformly.
            totalPosition += vec4(vec4(position, 1.0) * skin, 1.0) * weights[i];
            totalNormal += (vec4(normal, 0.0) * skin) * weights[i];
            totalTangent += (vec4(tangent.xyz, 0.0) * skin) * weights[i];
            validBoneFound = true;
        }

    }
    
    if (!validBoneFound)
    {
		totalPosition = vec4(position, 1.0);
        totalNormal = normal;
        totalTangent = tangent.xyz;
	}

    vec4 worldPos = instance.model * vec4(totalPosition.xyz, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(instance.model)));
    vec3 worldNormal = normalize(normalMatrix * normalize(totalNormal));

    // A mirroring model matrix flips the bitangent's direction relative to the surface. A zero
    // tangent stays zero, which leaves normal mapping off in the fragment shader.
    float handedness = determinant(mat3(instance.model)) < 0.0 ? -tangent.w : tangent.w;
    fragWorldTangent = vec4(mat3(instance.model) * totalTangent, handedness);

    gl_Position = uniforms.projectionView * worldPos;

    fragColor = color;
    fragTint = instance.tint;
    fragTexCoord = texCoord;
    textureIndex = instance.textureIndices;
    textureIndex2 = instance.textureIndices2;
    baseColorFactor = instance.baseColorFactor;
    metallicRoughnessFactor = instance.metallicRoughnessFactor;
    emissiveFactor = instance.emissiveFactor;
    instanceIndex = INSTANCE_ID;
    fragWorldPos = worldPos.xyz;
    fragWorldNormal = worldNormal;
}
