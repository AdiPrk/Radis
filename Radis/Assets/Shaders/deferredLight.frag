#version 460

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b)
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
#endif

// Camera uniforms
UBO_LAYOUT(0, 0) uniform Uniforms
{
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
} uniforms;

// G-Buffer textures
layout(set = 0, binding = 1) uniform sampler2D gAlbedo;   // R8G8B8A8_SRGB
layout(set = 0, binding = 2) uniform sampler2D gNormal;   // R16G16_SFLOAT (octahedral)
layout(set = 0, binding = 3) uniform sampler2D gPBR;      // R8G8B8A8_UNORM
layout(set = 0, binding = 4) uniform sampler2D gEmissive; // B10G11R11_UFLOAT_PACK32 (HDR)
layout(set = 0, binding = 5) uniform sampler2D gDepth;

// Compressed Light data (64 bytes instead of 80)
struct Light {
    vec4 positionRadius;    // xyz = position, w = radius
    vec4 colorIntensity;    // xyz = color, w = intensity
    vec4 directionInner;    // xyz = direction, w = innerCone
    vec4 outerConeType;     // x = outerCone, y = type (0=dir, 1=point, 2=spot), zw = padding
};

#define MAX_LIGHTS 10000
SSBO_LAYOUT(0, 6) readonly buffer LightData {
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
    float num = NdotV;
    float den = NdotV * (1.0 - k) + k;
    return num / den;
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
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

// Reconstruct world position from depth
vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);    
    vec4 worldPos = uniforms.invProjView * clipPos;
    return worldPos.xyz / worldPos.w;
}

// Octahedral normal decoding
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
    // Sample G-Buffer
    vec4 albedoSample = texture(gAlbedo, fragTexCoord);
    vec2 normalSample = texture(gNormal, fragTexCoord).rg;
    vec4 pbrSample = texture(gPBR, fragTexCoord);
    vec3 emissiveSample = texture(gEmissive, fragTexCoord).rgb;
    float depth = texture(gDepth, fragTexCoord).r;

    // Early out for sky/background
    if (depth >= 1.0)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Unpack G-Buffer data
    vec3 albedo = albedoSample.rgb;
    vec3 N = OctDecode(normalSample);
    float metallic = pbrSample.r;
    float roughness = pbrSample.g;
    float ao = pbrSample.b;
    vec3 emissive = emissiveSample;

    // Reconstruct world position from depth
    vec3 worldPos = ReconstructWorldPos(fragTexCoord, depth);
    
    // View direction
    vec3 V = normalize(uniforms.cameraPos - worldPos);

    // Accumulate lighting
    vec3 Lo = vec3(0.0);

    for (uint i = 0; i < lightData.lightCount; ++i)
    {
        Light light = lightData.lights[i];
        
        vec3 lightPos = light.positionRadius.xyz;
        float lightRadius = light.positionRadius.w;
        vec3 lightColor = light.colorIntensity.xyz;
        float lightIntensity = light.colorIntensity.w;
        vec3 lightDir = light.directionInner.xyz;
        float innerCone = light.directionInner.w;
        float outerCone = light.outerConeType.x;
        int lightType = int(light.outerConeType.y);
        
        vec3 L;
        float attenuation = 1.0;

        if (lightType == 0) 
        {
            // Directional light
            L = normalize(-lightDir);
        }
        else 
        {
            // Point / Spot light
            vec3 toLight = lightPos - worldPos;
            float dist = length(toLight);
            
            // Early out if outside light radius
            if (dist >= lightRadius)
                continue;
                
            L = toLight / dist;
            attenuation = 1.0 - dist / lightRadius;
            attenuation *= attenuation;
        }

        if (lightType == 2) 
        {
            // Spot light cone attenuation
            float spotFactor = dot(L, normalize(-lightDir));
            if (spotFactor < outerCone)
                continue;
            float smoothS = smoothstep(outerCone, innerCone, spotFactor);
            attenuation *= smoothS;
        }

        vec3 lightCol = lightColor * lightIntensity * attenuation;
        Lo += computePBRLight(albedo, metallic, roughness, N, V, L, lightCol);
    }

    // Ambient lighting with AO
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    // Final color (HDR)
    vec3 color = (Lo * ao) + ambient + emissive;

    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}