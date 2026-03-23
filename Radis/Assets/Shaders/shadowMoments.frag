#version 460

layout(location = 0) in float vLightViewZ;
layout(location = 0) out vec4 outMoments;

void main()
{
    float z = gl_FragCoord.z;
    float z2 = z * z;
    outMoments = vec4(z, z2, z2 * z, z2 * z2);
}