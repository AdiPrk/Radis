#version 460
#ifdef VULKAN
	#extension GL_EXT_nonuniform_qualifier : require
#else
	#extension GL_ARB_bindless_texture : require
#endif

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec4 fragTint;
layout(location = 2) in vec3 fragWorldNormal;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) flat in uvec4 textureIndicies;
layout(location = 5) flat in uvec4 textureIndicies2;
layout(location = 6) flat in vec4 baseColorFactor;
layout(location = 7) flat in vec4 metallicRoughnessFactor;
layout(location = 8) flat in vec4 emissiveFactor;
layout(location = 9) flat in uint instanceIndex;
layout(location = 10) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const uint INVALID_TEXTURE_INDEX = 10001;

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
    vec3 cameraPos;
} uniforms;

#ifdef VULKAN
	layout(set = 0, binding = 3) uniform sampler2D uTextures[];
#else
	SSBO_LAYOUT(0, 3) readonly buffer TexHandles {
		uvec2 colorHandle[];
	} texHandles;
#endif

struct Light {
    vec3 position;
    float radius;      // For point/spot attenuation
    vec3 color;
    float intensity;
    vec3 direction;
    float innerCone;   // for spot
    float outerCone;   // for spot
    int type;          // 0=dir, 1=point, 2=spot
};

#define MAX_LIGHTS 10000
SSBO_LAYOUT(0, 4) readonly buffer LightData {
    uint lightCount;
    Light lights[MAX_LIGHTS];
} lightData;

// --- PBR Implementation ---

// GGX Trowbridge-Reitz
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float den = (NdotH2 * (a2 - 1.0) + 1.0);
    den = PI * den * den;
    return num / (den + 0.0001); // Epsilon
}

// Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float den = NdotV * (1.0 - k) + k;
    return num / den;
}

// Smith's method
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel-Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Main PBR function
vec3 computePBRLight(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 lightColor)
{
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 H = normalize(V + L);

    // Cook-Torrance BRDF
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

    // Final radiance
    return (diffuse + specular) * NdotL * lightColor;
}

// -------------------------
vec4 SampleTexture(uint texIndex, vec2 uv)
{
#ifdef VULKAN
    return texture(uTextures[texIndex], uv);
#else
    uvec2 handle = texHandles.colorHandle[texIndex];
    if (handle == uvec2(0, 0))
    {
        return vec4(1.0);
    }

    return texture(sampler2D(handle), uv);
#endif
}

void main()
{
	// Base Color
	vec4 baseColor = vec4(fragColor * fragTint.rgb, fragTint.a) * baseColorFactor;
	if (textureIndicies.x != INVALID_TEXTURE_INDEX)
	{
        baseColor = SampleTexture(textureIndicies.x, fragTexCoord) * fragTint * baseColorFactor;
	}
    vec3 albedo = baseColor.rgb;

    // Metallic
    float metallic = metallicRoughnessFactor.x;
    if (textureIndicies.z != INVALID_TEXTURE_INDEX)
    {
        metallic = SampleTexture(textureIndicies.z, fragTexCoord).b;
    }

    // Roughness
    float roughness = metallicRoughnessFactor.y;
    if (textureIndicies.w != INVALID_TEXTURE_INDEX)
    {
        roughness = SampleTexture(textureIndicies.w, fragTexCoord).g;
        roughness = clamp(roughness, 0.04, 1.0);
    }

    // Normal mapping (unused, tbd later)
    vec3 N = normalize(fragWorldNormal);

    // Ambient Occlusion
    float ao = 1.0;
    if (textureIndicies2.x != INVALID_TEXTURE_INDEX)
    {
        ao = clamp(SampleTexture(textureIndicies2.x, fragTexCoord).r, 0.0, 1.0);        
    }

    // Emissive
    vec3 emissive = emissiveFactor.rgb;
    if (textureIndicies2.y != INVALID_TEXTURE_INDEX)
    {
        emissive *= SampleTexture(textureIndicies2.y, fragTexCoord).rgb;
    }

    // Lighting
    vec3 V = normalize(uniforms.cameraPos - fragWorldPos);
    vec3 Lo = vec3(0.0);

    for (uint i = 0; i < lightData.lightCount; ++i)
    {
        Light light = lightData.lights[i];
        vec3 L;
        float attenuation = 1.0;

        if (light.type == 0) {
            // Directional light
            L = normalize(-light.direction);
        }
        else {
            // Point / Spot
            vec3 toLight = light.position - fragWorldPos;
            float dist = length(toLight);
            L = normalize(toLight);
            attenuation = clamp(1.0 - dist / light.radius, 0.0, 1.0);
            attenuation *= attenuation; // smoother
        }

        if (light.type == 2) {
            // Spot light cone
            float spotFactor = dot(L, -light.direction);
            float smoothS = smoothstep(light.outerCone, light.innerCone, spotFactor);
            attenuation *= smoothS;
        }

        vec3 lightCol = light.color * light.intensity * attenuation;
        Lo += computePBRLight(albedo, metallic, roughness, N, V, L, lightCol);
    }

    // Apply ambient & AO
    ao = 1.0;
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = (Lo * ao) + ambient + emissive;

    // Tone map + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, baseColor.a);
    if (outColor.a < 0.1)
	{   
		discard;
	}
}