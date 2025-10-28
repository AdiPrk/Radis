#version 450

// Per Vertex Inputs
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;

// Per Instance Inputs
layout(location = 4) in mat4 iModel;

// Uniforms! (temporary hard coded macro)
#ifdef VULKAN
layout(set = 0, binding = 0) uniform Uniforms {
    mat4 projectionView;
    mat4 projection;
    mat4 view;
} uniforms;
#else
layout (std140, binding = 0) uniform Matrices
{
    mat4 projectionView;
    mat4 projection;
    mat4 view;
} uniforms;
#endif

// Outputs
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec4 fragTint;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec2 fragTexCoord;

void main() 
{
    gl_Position = uniforms.projectionView * iModel * vec4(position, 1.0);

    fragColor = color;
    fragTint = vec4(1.0);
    fragNormal = normal;
    fragTexCoord = texCoord;
}