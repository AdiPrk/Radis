#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneHDR;

layout(push_constant) uniform PushConstants {
    float exposure;
};

void main()
{
    vec3 color = texture(sceneHDR, fragTexCoord).rgb;

    // Tone mapping w/ exposure control
    color = (exposure * color) / (exposure * color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}