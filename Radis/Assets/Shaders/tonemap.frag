#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneHDR;
layout(set = 0, binding = 1) uniform sampler2D bloomTex;
layout(set = 0, binding = 2) uniform sampler2D dirtTex;

layout(push_constant) uniform PushConstants {
    float exposure;
    float bloomIntensity;
    float dirtMaskIntensity;
};

// ACES Filmic Tonemapping curve
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    vec3 hdr = texture(sceneHDR, fragTexCoord).rgb;
    vec3 blm = texture(bloomTex, fragTexCoord).rgb;
    vec3 drt = texture(dirtTex, vec2(fragTexCoord.x, 1.0f - fragTexCoord.y)).rgb * dirtMaskIntensity;

    vec3 color = mix(hdr, blm + blm*drt, vec3(bloomIntensity));

    // Exposure
    color *= exposure;

    // ACES Tonemapping
    color = ACESFilm(color);

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    // GG
    outColor = vec4(color, 1.0);
}