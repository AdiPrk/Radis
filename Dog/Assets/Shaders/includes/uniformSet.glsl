#include "glvkDefines.glsl"

UBO_LAYOUT(0, 0) uniform Uniforms
{
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    vec3 cameraPos;
    vec3 lightDir;
    vec3 lightColor;
    float lightIntensity;
} uniforms;