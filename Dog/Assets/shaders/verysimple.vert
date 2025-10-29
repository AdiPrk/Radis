#version 450

// Per Vertex Inputs
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in ivec4 boneIds;
layout(location = 5) in vec4 weights;

// Per Instance Inputs (For vulkan: Set 0 binding 1)
layout(location = 6) in mat4 iModel;
layout(location = 10) in vec4 iTint;
layout(location = 11) in uint iTextureIndex;
layout(location = 12) in uint iBoneOffset;

/*
struct Instance
{
	mat4 model;
	vec4 tint;
	uint textureIndex;
	uint boneOffset;
}
*/

struct VQS {
    vec4 rotation;    // Quat
    vec3 translation; // Vector
    vec3 scale;       // Scale
};

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b)
    #define INSTANCE_ID gl_InstanceIndex
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
    #define INSTANCE_ID gl_InstanceID
#endif

// Uniforms!
UBO_LAYOUT(0, 0) uniform Uniforms
{
    mat4 projectionView;
    mat4 projection;
    mat4 view;
} uniforms;

SSBO_LAYOUT(0, 2) readonly buffer BoneBuffer
{
    VQS finalBoneVQS[10000];
} animationData;

// Outputs -----------------------------------------
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec4 fragTint;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) flat out uint textureIndex;
layout(location = 5) flat out uint instanceIndex;
// -------------------------------------------------

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
    
    bool validBoneFound = false;
    if (iBoneOffset != 10001)
    {
        for (uint i = 0; i < 4 ; i++)
        {
            if(boneIds[i] == -1) continue;

            VQS transform = animationData.finalBoneVQS[iBoneOffset + boneIds[i]];

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

    gl_Position = uniforms.projectionView * iModel * vec4(totalPosition.xyz, 1.0);

    fragColor = color;
    fragTint = iTint;
    fragNormal = normalize(totalNormal);
    fragTexCoord = texCoord;
    textureIndex = iTextureIndex;
    instanceIndex = INSTANCE_ID;
}