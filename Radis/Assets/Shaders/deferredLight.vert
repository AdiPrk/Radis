#version 460

// #ifdef VULKAN
// 	#extension GL_KHR_vulkan_glsl : enable
// #endif

// Fullscreen triangle - no vertex input needed
// Uses gl_VertexIndex to generate a fullscreen triangle

layout(location = 0) out vec2 fragTexCoord;

void main()
{
    // Generate fullscreen triangle vertices
    // Vertex 0: (-1, -1), UV (0, 0)
    // Vertex 1: ( 3, -1), UV (2, 0)
    // Vertex 2: (-1,  3), UV (0, 2)
    
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragTexCoord * 2.0 - 1.0, 0.0, 1.0);
}