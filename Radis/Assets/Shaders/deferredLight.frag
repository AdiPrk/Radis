#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

const float PI     = 3.14159265359;
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

// MSM shadow inputs
layout(set = 0, binding = 7) uniform sampler2D shadowMoments;

UBO_LAYOUT(0, 8) uniform ShadowParams
{
    mat4 lightViewProj;
    mat4 lightView;
    vec4 zParams;   // (z0, z1, invRange, alpha)
    vec4 mapParams; // (invW, invH, blurRadius, unused)
} sh;

layout(set = 0, binding = 9) uniform sampler2D envMap;
layout(set = 0, binding = 10) uniform sampler2D irradianceMap;

layout(push_constant) uniform PushConstants {
    int useIrrDiffuse;
};

// ---------------------------------------------------------------------------
// PBR helpers
// ---------------------------------------------------------------------------

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
    float f5 = f2 * f2 * f;
    return F0 + (1.0 - F0) * f5;
}

vec3 computePBRLight(vec3 albedo, float metallic, float roughness,
                     vec3 N, vec3 V, vec3 L, vec3 lightColor)
{
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
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
    vec3 diffuse  = kD * albedo * INV_PI;

    return (diffuse + specular) * NdotL * lightColor;
}

// ---------------------------------------------------------------------------
// IBL diffuse
// ---------------------------------------------------------------------------

// Inside-sphere equirectangular UV
vec2 uvOf(vec3 dir)
{
    return vec2(
        0.5 - atan(dir.z, dir.x) * (0.5 * INV_PI),
        asin(clamp(dir.y, -1.0, 1.0)) * INV_PI + 0.5
    );
}

// IBL diffuse term
vec3 computeIBLDiffuse(vec3 albedo, float metallic, float ao, vec3 N)
{
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    // Use constant-incidence Fresnel for the hemisphere-integrated kD.
    // Full angle-dependent kD would require a BRDF LUT integral (saved for IBL specular).
    vec3 kD = (1.0 - F0) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceMap, uvOf(N)).rgb;
    return kD * albedo * INV_PI * irradiance * ao;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    vec4 clipPos  = vec4(uv * 2.0 - 1.0, depth, 1.0);
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

// ---------------------------------------------------------------------------
// Skybox helpers
// ---------------------------------------------------------------------------

vec3 GetSkyDirection(vec2 uv)
{
    vec4 ndc      = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldPos = uniforms.invProjView * ndc;
    return normalize(worldPos.xyz / worldPos.w - uniforms.cameraPos);
}

// Outside-sphere UV
vec2 DirToEquirect(vec3 dir)
{
    const vec2 invAtan = vec2(0.1591, 0.3183); // (1/2pi, 1/pi)
    vec2 uv = vec2(atan(dir.z, dir.x), asin(clamp(dir.y, -1.0, 1.0)));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

void main()
{
    float depth = texture(gDepth, fragTexCoord).r;
    if (depth >= 1.0) // Skybox
    {
        vec3 dir    = GetSkyDirection(fragTexCoord);
        vec2 envUV  = DirToEquirect(dir);
        vec3 skyCol = texture(envMap, envUV).rgb;
        outColor = vec4(skyCol, 1.0);

        return;
    }

    // G-Buffer unpack
    vec4 albedoSample = texture(gAlbedo,   fragTexCoord);
    vec2 normalEnc    = texture(gNormal,   fragTexCoord).rg;
    vec4 pbrSample    = texture(gPBR,      fragTexCoord);
    vec3 emissive     = texture(gEmissive, fragTexCoord).rgb;

    vec3  albedo    = albedoSample.rgb;
    vec3  N         = OctDecode(normalEnc);
    float metallic  = pbrSample.r;
    float roughness = pbrSample.g;
    float ao        = pbrSample.b;

    vec3 worldPos = ReconstructWorldPos(fragTexCoord, depth);
    vec3 V        = normalize(uniforms.cameraPos - worldPos);

    // Direct lighting; directional lights only (point/spot handled in lightVolume pass)
    vec3 Lo = vec3(0.0);

    uint lightCount = lightData.lightCount;
    for (uint i = 0; i < lightCount; ++i)
    {
        Light light     = lightData.lights[i];
        float lightType = light.outerConeType.y;
        if (lightType != 0.0) break;

        vec3 L        = normalize(-light.directionInner.xyz);
        vec3 lightCol = light.colorIntensity.xyz * light.colorIntensity.w;

        Lo += computePBRLight(albedo, metallic, roughness, N, V, L, lightCol);
    }

    // IBL diffuse
    if (useIrrDiffuse == 0) // Regular lighting
    {
        vec3 iblDiffuse = computeIBLDiffuse(albedo, metallic, ao, N);
        vec3 finalColor = Lo + iblDiffuse + emissive;
        
        outColor = vec4(finalColor, 1.0);
    }
    else if (useIrrDiffuse == 1) // Raw irradiance map
    {
        vec3 rawIrradiance = texture(irradianceMap, uvOf(N)).rgb;
        outColor = vec4(rawIrradiance, 1.0);
    }
    else if (useIrrDiffuse == 2) // Normals
    {
        outColor = vec4(N, 1.0);
    }
    else if (useIrrDiffuse == 3) // Splitscreen (Left = IrrMap Diffuse, Right = Old Ambient)
    {
        vec3 iblDiffuse = computeIBLDiffuse(albedo, metallic, ao, N);
        vec3 finalColor = Lo + iblDiffuse + emissive;
        
        if (fragTexCoord.x > 0.5) {
            //finalColor = Lo + emissive;
            vec3 ambient = vec3(0.01) * albedo * ao;
            finalColor = (Lo * ao) + ambient + emissive;
        }

        outColor = vec4(finalColor, 1.0);
    }
    else // No ambient
    {
        vec3 color = Lo + emissive;
        outColor = vec4(color, 1.0);
    }
}