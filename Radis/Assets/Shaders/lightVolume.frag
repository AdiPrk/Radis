#version 460

layout(location = 0) flat in uint lightIndex;
layout(location = 0) out vec4 outColor;

const float PI     = 3.14159265359;
const float INV_PI = 0.31830988618;

layout(push_constant) uniform PushConstants {
    uint directionalLightCount;
    uint debugMode;
    vec2 invViewport;
} pc;

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
    int frameCount;
} uniforms;

layout(set = 0, binding = 1) uniform sampler2D gAlbedo;
layout(set = 0, binding = 2) uniform sampler2D gNormal;
layout(set = 0, binding = 3) uniform sampler2D gPBR;
layout(set = 0, binding = 4) uniform sampler2D gEmissive;
layout(set = 0, binding = 5) uniform sampler2D gDepth;

struct Light {
    vec4 positionRadius;
    vec4 colorIntensity;
    vec4 directionInner;
    vec4 outerConeType;
};

#define MAX_LIGHTS 100000
layout(set = 0, binding = 6, std430) readonly buffer LightData {
    uint lightCount;
    Light lights[MAX_LIGHTS];
} lightData;

// blurred moments + params
layout(set=0, binding=7) uniform sampler2D shadowMoments;

layout(set=0, binding=8) uniform ShadowParams {
    mat4 lightViewProj;
    mat4 lightView;
    vec4 zParams;   // (z0, z1, invRange, alpha)
    vec4 mapParams; // (invW, invH, blurRadius, pad)
} sh;

// Optimized GGX D term
float D_GGX(float NdotH, float a2)
{
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-7);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2)
{
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL + 1e-7);
}

vec3 F_Schlick(float VdotH, vec3 F0)
{
    float f  = 1.0 - VdotH;
    float f2 = f * f;
    return F0 + (1.0 - F0) * (f2 * f2 * f);
}

vec3 computePBRLight(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 lightColor)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    vec3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float a  = roughness * roughness;
    float a2 = a * a;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D   = D_GGX(NdotH, a2);
    float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a2);
    vec3  F   = F_Schlick(VdotH, F0);

    vec3 specular = D * Vis * F;
    vec3 kD       = (1.0 - F) * (1.0 - metallic);

    return (kD * albedo * INV_PI + specular) * NdotL * lightColor;
}

vec3 ReconstructWorldPosFromNDC(vec2 ndc, float depth)
{
    vec4 clipPos  = vec4(ndc, depth, 1.0);
    vec4 worldPos = uniforms.invProjView * clipPos;
    return worldPos.xyz / worldPos.w;
}

vec3 OctDecode(vec2 f)
{
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.xy += mix(vec2(t), vec2(-t), greaterThanEqual(n.xy, vec2(0.0)));
    return normalize(n);
}

void main()
{
    ivec2 pix = ivec2(gl_FragCoord.xy);

    float depth = texelFetch(gDepth, pix, 0).r;
    if (depth > 0.999999) discard;

    if (pc.debugMode == 2u)
    {
        outColor = vec4(0.04, 0.02, 0.0, 0.0);
        return;
    }

    vec2 uv  = (vec2(pix) + 0.5) * pc.invViewport;
    vec2 ndc = uv * 2.0 - 1.0;

    Light light     = lightData.lights[lightIndex];
    vec3  lightPos  = light.positionRadius.xyz;
    float range     = light.positionRadius.w;
    float rangeInv  = 1.0 / max(range, 1e-6);
    float range2    = range * range;

    vec3 worldPos = ReconstructWorldPosFromNDC(ndc, depth);

    vec3  toLight = lightPos - worldPos;
    float dist2   = dot(toLight, toLight);

    if (dist2 >= range2) { outColor = vec4(0.0); return; }

    float invDist = inversesqrt(max(dist2, 1e-12));
    float dist    = dist2 * invDist;
    vec3  L       = toLight * invDist;

    if (pc.debugMode == 1u)
    {
        float hue = fract(float(lightIndex) * 0.618033988749895);
        vec3 tint = clamp(vec3(
            abs(hue * 6.0 - 3.0) - 1.0,
            2.0 - abs(hue * 6.0 - 2.0),
            2.0 - abs(hue * 6.0 - 4.0)
        ), 0.0, 1.0);

        outColor = vec4(tint * (1.0 - dist * rangeInv) * 0.3, 0.0);
        return;
    }

    vec4 albedoAO  = texelFetch(gAlbedo, pix, 0);
    vec4 pbrSample = texelFetch(gPBR,    pix, 0);
    vec2 normalEnc = texelFetch(gNormal, pix, 0).rg;

    vec3  albedo    = albedoAO.rgb;
    vec3  N         = OctDecode(normalEnc);
    float metallic  = pbrSample.r;
    float roughness = pbrSample.g;
    float ao        = pbrSample.b;

    vec3 V = normalize(uniforms.cameraPos - worldPos);

    float distNorm    = dist * rangeInv;
    float oneMinus    = 1.0 - distNorm;
    float attenuation = oneMinus * oneMinus;

    float lightType = light.outerConeType.y;
    if (lightType == 2.0)
    {
        vec3  lightDir  = light.directionInner.xyz;
        float innerCone = light.directionInner.w;
        float outerCone = light.outerConeType.x;

        vec3  spotDir   = normalize(-lightDir);
        float spotFactor = dot(L, spotDir);

        if (spotFactor < outerCone) { outColor = vec4(0.0); return; }
        attenuation *= smoothstep(outerCone, innerCone, spotFactor);
    }

    vec3  lightColor     = light.colorIntensity.xyz;
    float lightIntensity = light.colorIntensity.w;
    vec3  lightCol       = lightColor * lightIntensity * attenuation;

    vec3 Lo = computePBRLight(albedo, metallic, roughness, N, V, L, lightCol);

    outColor = vec4(Lo * ao, 0.0);
}
