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
    vec3 lightDir;
    vec4 lightColor; // w is lightIntensity
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

#define MAX_LIGHTS 1000
SSBO_LAYOUT(0, 4) uniform LightData {
    Light lights[1000];
    uint lightCount;
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
vec3 pbrMetallicRoughness(vec3 albedo, float metallic, float roughness, 
                          vec3 N, vec3 V, vec3 L, vec3 lightColor, float lightIntensity)
{
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 H = normalize(V + L);

    // Cook-Torrance BRDF
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // Specular
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3 F = FresnelSchlick(VdotH, F0);
    
    vec3 numSpecular = D * G * F;
    float denSpecular = 4.0 * NdotV * NdotL + 0.0001; // Epsilon
    vec3 specular = numSpecular / denSpecular;

    // Diffuse
    vec3 kD = (1.0 - metallic) * (1.0 - F); // Use F for energy conservation
    vec3 diffuse = kD * albedo / PI;

    // Final radiance
    vec3 Lo = (diffuse + specular) * NdotL * lightColor * lightIntensity;
    return Lo;
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

// Helper to compute TBN per-fragment from dFdx/dFdy
mat3 computeTBN(vec3 N, vec2 uv, vec3 P)
{
    // derivatives of position and UV
    vec3 dp1 = dFdx(P);
    vec3 dp2 = dFdy(P);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    // Compute tangent
    float denom = duv1.x * duv2.y - duv2.x * duv1.y;
    // if denom is zero, fallback to a stable basis
    if (abs(denom) < 1e-6)
    {
        // fallback: build arbitrary tangent
        vec3 tangent = normalize(cross(vec3(0.0, 1.0, 0.0), N));
        if (length(tangent) < 1e-3) tangent = normalize(cross(vec3(1.0, 0.0, 0.0), N));
        vec3 bitangent = normalize(cross(N, tangent));
        return mat3(tangent, bitangent, N);
    }

    vec3 tangent = normalize((dp1 * duv2.y - dp2 * duv1.y) / denom);
    vec3 bitangent = normalize(cross(N, tangent));

    return mat3(tangent, bitangent, N);
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

    // Normal mapping
    vec3 N = normalize(fragWorldNormal);
    if (textureIndicies.y != INVALID_TEXTURE_INDEX)
    {
        vec3 mapN = SampleTexture(textureIndicies.y, fragTexCoord).xyz * 2.0 - 1.0;
        N = normalize(computeTBN(N, fragTexCoord, fragWorldPos) * mapN);
    }

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
    vec3 L = normalize(-uniforms.lightDir); // Direction to the light
   
    // Get direct lighting
    vec3 Lo = pbrMetallicRoughness(albedo, metallic, roughness, N, V, L, uniforms.lightColor.rgb, uniforms.lightColor.a);
    
    // Apply ambient occlusion to direct lighting
    Lo *= ao;

    // Add simple ambient light
    vec3 ambient = vec3(0.05) * albedo * ao;
    
    // Combine lighting + emissive
    vec3 color = Lo + ambient + emissive;

    // Tonemapping & Gamma
    color = color / (color + vec3(1.0)); // Reinhard
    color = pow(color, vec3(1.0/2.2));   // gamma
    
    // Way to toggle pbr on and off
    if (uniforms.lightColor.a > 0.0) 
    {
	    outColor = vec4(color, baseColor.a);
    }
    else 
    {
        outColor = baseColor;
    }

    if (outColor.a < 0.1)
	{
		discard;
	}
}