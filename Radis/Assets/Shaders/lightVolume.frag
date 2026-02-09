#version 460

layout(location = 0) flat in uint lightIndex;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

layout(push_constant) uniform PushConstants {
    uint directionalLightCount;
    uint debugMode;   // 0=normal, 1=volume tint, 2=density heatmap
    uint _pad[2];
} pc;

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
} uniforms;

// G-Buffer textures
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

#define MAX_LIGHTS 200000
layout(set = 0, binding = 6, std430) readonly buffer LightData {
    uint lightCount;
    Light lights[MAX_LIGHTS];
} lightData;

// --- PBR Functions ---

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float den = (NdotH2 * (a2 - 1.0) + 1.0);
    den = PI * den * den;
    return num / (den + 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 computePBRLight(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 lightColor)
{
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3 F = FresnelSchlick(VdotH, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

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
    if (n.z < 0.0)
    {
        vec2 wrapped = (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
        n.xy = wrapped;
    }
    return normalize(n);
}

void main()
{
    // lightIndex was computed in the vertex shader and passed via flat interpolation
    Light light = lightData.lights[lightIndex];

    vec3  lightPos       = light.positionRadius.xyz;
    float range          = light.positionRadius.w;
    vec3  lightColor     = light.colorIntensity.xyz;
    float lightIntensity = light.colorIntensity.w;
    float lightType      = light.outerConeType.y;

    // Screen UV from fragment position
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(gAlbedo, 0));

    float depth = texture(gDepth, uv).r;
    if (depth >= 1.0)
    {
        outColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    vec3 worldPos = ReconstructWorldPos(uv, depth);

    float dist = distance(worldPos, lightPos);
    if (dist >= range)
    {
        outColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // --- Debug Mode 2: Density heatmap (count overlapping lights) ---
    if (pc.debugMode == 2)
    {
        outColor = vec4(0.04, 0.02, 0.0, 0.0);
        return;
    }

    // --- Debug Mode 1: Volume tint (flat color per light) ---
    if (pc.debugMode == 1)
    {
        float hue = fract(float(lightIndex) * 0.618033988749895);
        vec3 tint = vec3(
            abs(hue * 6.0 - 3.0) - 1.0,
            2.0 - abs(hue * 6.0 - 2.0),
            2.0 - abs(hue * 6.0 - 4.0)
        );
        tint = clamp(tint, 0.0, 1.0);
        float falloff = 1.0 - dist / range;
        outColor = vec4(tint * falloff * 0.3, 0.0);
        return;
    }

    // --- Normal rendering (Mode 0) ---
    vec3 albedo = texture(gAlbedo, uv).rgb;
    vec3 N = OctDecode(texture(gNormal, uv).rg);
    vec4 pbrSample = texture(gPBR, uv);
    float metallic  = pbrSample.r;
    float roughness = pbrSample.g;
    float ao        = pbrSample.b;

    vec3 V = normalize(uniforms.cameraPos - worldPos);
    vec3 L = normalize(lightPos - worldPos);

    // Attenuation matching the original deferredLight.frag formula
    float attenuation = 1.0 - dist / range;
    attenuation *= attenuation;

    // Spot light cone attenuation
    if (lightType == 2.0)
    {
        vec3  lightDir = light.directionInner.xyz;
        float innerCone = light.directionInner.w;
        float outerCone = light.outerConeType.x;
        float spotFactor = dot(L, normalize(-lightDir));
        if (spotFactor < outerCone)
        {
            outColor = vec4(0.0, 0.0, 0.0, 0.0);
            return;
        }
        attenuation *= smoothstep(outerCone, innerCone, spotFactor);
    }

    vec3 lightCol = lightColor * lightIntensity * attenuation;
    vec3 Lo = computePBRLight(albedo, metallic, roughness, N, V, L, lightCol);

    outColor = vec4(Lo * ao, 0.0);
}