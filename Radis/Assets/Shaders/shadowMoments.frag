#version 460

layout(location = 0) in float vLightViewZ;
layout(location = 0) out vec4 outMoments;

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b, std140)
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
#endif

UBO_LAYOUT(0, 0) uniform ShadowUniforms
{
    mat4 lightViewProj;
    mat4 lightView;
    float z0;
    float z1;
    float pad0;
    float pad1;
} sh;

void main()
{
    float zRel = (vLightViewZ - sh.z0) / max(sh.z1 - sh.z0, 1e-6);
    zRel = clamp(zRel, 0.0, 1.0);
    
    float z2 = zRel * zRel;
    outMoments = vec4(zRel, z2, z2 * zRel, z2 * z2);
}