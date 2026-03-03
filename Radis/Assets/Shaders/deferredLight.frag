#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float INV_PI = 0.31830988618;

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b)
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
#endif

UBO_LAYOUT(0, 0) uniform Uniforms
{
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
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
SSBO_LAYOUT(0, 6) readonly buffer LightData {
    uint lightCount;
    Light lights[MAX_LIGHTS];
} lightData;

// NEW: MSM shadow inputs
layout(set=0, binding=7) uniform sampler2D shadowMoments;

UBO_LAYOUT(0, 8) uniform ShadowParams
{
    mat4 lightViewProj;
    mat4 lightView;
    vec4 zParams;   // (z0, z1, invRange, alpha)
    vec4 mapParams; // (invW, invH, blurRadius, unused)
} sh;

// --- PBR helpers (your originals) ---
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
    float f = 1.0 - VdotH;
    float f2 = f * f;
    float f5 = f2 * f2 * f;
    return F0 + (1.0 - F0) * f5;
}

vec3 computePBRLight(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 lightColor)
{
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float a = roughness * roughness;
    float a2 = a * a;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = D_GGX(NdotH, a2);
    float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a2);
    vec3 F = F_Schlick(VdotH, F0);

    vec3 specular = D * Vis * F;
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo * INV_PI;

    return (diffuse + specular) * NdotL * lightColor;
}

vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
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

// MSM Hamburger 4 (same as you already have)
float MSM_Hamburger4(vec4 b, float zf, float alpha)
{
    vec4 bp = mix(b, vec4(0.5), alpha);
    bp = clamp(bp, 0.0, 1.0);

    float m11 = 1.0;
    float m12 = bp.x;
    float m13 = bp.y;
    float m22 = bp.y;
    float m23 = bp.z;
    float m33 = bp.w;

    float z1 = 1.0;
    float z2 = zf;
    float z3 = zf*zf;

    const float EPS = 1e-6;

    float a = sqrt(max(m11, EPS));
    float bL = m12 / a;
    float cL = m13 / a;

    float d2 = m22 - bL*bL;
    float d  = sqrt(max(d2, EPS));

    float eL = (m23 - bL*cL) / d;

    float f2 = m33 - cL*cL - eL*eL;
    float f  = sqrt(max(f2, EPS));

    float chat1 = z1 / a;
    float chat2 = (z2 - bL*chat1) / d;
    float chat3 = (z3 - cL*chat1 - eL*chat2) / f;

    float c3 = chat3 / f;
    float c2 = (chat2 - eL*c3) / d;
    float c1 = (chat1 - bL*c2 - cL*c3) / a;

    float A = c3;
    float B = c2;
    float C = c1;

    if (abs(A) < 1e-8)
    {
        float mu  = bp.x;
        float var = max(bp.y - mu*mu, 0.0);
        float dmu = zf - mu;
        float p   = var / (var + dmu*dmu + EPS);
        return clamp(1.0 - p, 0.0, 1.0);
    }

    float disc = max(B*B - 4.0*A*C, 0.0);
    float sdisc = sqrt(disc);

    float r1 = (-B - sdisc) / (2.0*A);
    float r2 = (-B + sdisc) / (2.0*A);

    float zLo = min(r1, r2);
    float zHi = max(r1, r2);

    if (zf <= zLo) return 0.0;

    if (zf <= zHi)
    {
        float numer = zf*zHi - bp.x*(zf + zHi) + bp.y;
        float denom = (zHi - zLo) * (zf - zLo);
        return clamp(numer / max(denom, EPS), 0.0, 1.0);
    }
    else
    {
        float numer = zLo*zHi - bp.x*(zLo + zHi) + bp.y;
        float denom = (zf - zLo) * (zf - zHi);
        return clamp(1.0 - numer / max(denom, EPS), 0.0, 1.0);
    }
}

float ComputeDirectionalShadow(vec3 worldPos)
{
    vec4 lp = sh.lightViewProj * vec4(worldPos, 1.0);
    vec3 ndc = lp.xyz / max(lp.w, 1e-6);
    vec2 uv = ndc.xy * 0.5 + 0.5;

    // out of bounds => lit
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        return 1.0;

    // Compute zf in LIGHT VIEW space (matches shadow pass)
    float lightViewZ = -(sh.lightView * vec4(worldPos, 1.0)).z;
    float zf = (lightViewZ - sh.zParams.x) * sh.zParams.z; // (z - z0) / (z1 - z0)
    zf = clamp(zf, 0.0, 1.0);

    vec4 m = texture(shadowMoments, uv);
    float G = MSM_Hamburger4(m, zf, sh.zParams.w);
    float shadow = 1.0 - G; // lit factor
    return clamp(shadow, 0.0, 1.0);
}

void main()
{
    float depth = texture(gDepth, fragTexCoord).r;
    if (depth >= 1.0)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 albedoSample = texture(gAlbedo, fragTexCoord);
    vec4 normalPBR    = vec4(texture(gNormal, fragTexCoord).rg, texture(gPBR, fragTexCoord).rg);
    vec3 emissive     = texture(gEmissive, fragTexCoord).rgb;

    vec3  albedo    = albedoSample.rgb;
    vec3  N         = OctDecode(normalPBR.xy);
    float metallic  = normalPBR.z;
    float roughness = normalPBR.w;
    float ao        = texture(gPBR, fragTexCoord).b;

    vec3 worldPos = ReconstructWorldPos(fragTexCoord, depth);
    vec3 V = normalize(uniforms.cameraPos - worldPos);

    float shadow = ComputeDirectionalShadow(worldPos);

    vec3 Lo = vec3(0.0);

    uint lightCount = lightData.lightCount;
    for (uint i = 0; i < lightCount; ++i)
    {
        Light light = lightData.lights[i];
        float lightType = light.outerConeType.y;
        if (lightType != 0.0) break; // directional only

        vec3 L = normalize(-light.directionInner.xyz);
        vec3 lightCol = light.colorIntensity.xyz * light.colorIntensity.w;

        // Apply shadow to directional contribution
        Lo += computePBRLight(albedo, metallic, roughness, N, V, L, lightCol) * shadow;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = (Lo * ao) + ambient + emissive;

    outColor = vec4(color, 1.0);
}
