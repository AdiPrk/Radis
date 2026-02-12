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

// Optimized GGX - combined D*G term (Hammon 2017)
float D_GGX(float NdotH, float a2)
{
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-7);
}

// Optimized visibility term (Smith height-correlated)
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2)
{
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL + 1e-7);
}

// Fresnel-Schlick using fast approximation (no pow)
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
    float NdotV = max(dot(N, V), 1e-4);  // Clamp to avoid artifacts
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

void main()
{
    float depth = texture(gDepth, fragTexCoord).r;
    
    // Early out for sky/background
    if (depth >= 1.0)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // Batch G-Buffer reads
    vec4 albedoSample = texture(gAlbedo, fragTexCoord);
    vec4 normalPBR = vec4(texture(gNormal, fragTexCoord).rg, texture(gPBR, fragTexCoord).rg);
    vec4 pbrEmissive = vec4(texture(gPBR, fragTexCoord).ba, 0.0, 0.0);
    vec3 emissive = texture(gEmissive, fragTexCoord).rgb;
    
    vec3 albedo = albedoSample.rgb;
    vec3 N = OctDecode(normalPBR.xy);
    float metallic = normalPBR.z;
    float roughness = normalPBR.w;
    float ao = texture(gPBR, fragTexCoord).b;
    
    vec3 worldPos = ReconstructWorldPos(fragTexCoord, depth);
    vec3 V = normalize(uniforms.cameraPos - worldPos);
    
    vec3 Lo = vec3(0.0);
    
    // Process only directional lights (type == 0)
    // Directional lights are sorted first in buffer
    uint lightCount = lightData.lightCount;
    for (uint i = 0; i < lightCount; ++i)
    {
        Light light = lightData.lights[i];
        float lightType = light.outerConeType.y;
        
        // Early break: directional lights are first, once we hit non-directional, stop
        if (lightType != 0.0)
            break;
        
        vec3 L = normalize(-light.directionInner.xyz);
        vec3 lightCol = light.colorIntensity.xyz * light.colorIntensity.w;
        Lo += computePBRLight(albedo, metallic, roughness, N, V, L, lightCol);
    }
    
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = (Lo * ao) + ambient + emissive;
    
    outColor = vec4(color, 1.0);
}