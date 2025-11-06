#version 450

#extension GL_GOOGLE_include_directive : enable
#include "includes/uniformSet.glsl"
#include "includes/constants.glsl"

// VK Extensions
#ifdef VULKAN
	#extension GL_EXT_nonuniform_qualifier : require
#else
	#extension GL_ARB_bindless_texture : require
#endif

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec4 fragTint;
layout(location = 2) in vec3 fragWorldNormal;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) flat in uint textureIndex;
layout(location = 5) flat in uint instanceIndex;
layout(location = 6) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

#ifdef VULKAN
	layout(set = 0, binding = 3) uniform sampler2D uTextures[];
#else
	SSBO_LAYOUT(0, 3) readonly buffer TexHandles {
		uvec2 colorHandle[];
	} texHandles;
#endif

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

void main()
{
	// --- 1. Get Base Color (Albedo) ---
	vec4 baseColor = vec4(fragColor * fragTint.rgb, fragTint.a);

	if (textureIndex != INVALID_TEXTURE_INDEX)
	{
#ifdef VULKAN
		baseColor = texture(uTextures[textureIndex], fragTexCoord) * fragTint;
#else
		uvec2 colorH = texHandles.colorHandle[textureIndex];
		if (colorH != uvec2(0,0)) 
		{
			baseColor = texture(sampler2D(colorH), fragTexCoord) * fragTint;
		}
#endif
	}

    vec3 albedo = baseColor.rgb;
    float metallic = 0.1;
    float roughness = 0.5;

    vec3 N = normalize(fragWorldNormal);
    vec3 V = normalize(uniforms.cameraPos - fragWorldPos);
    vec3 L = normalize(-uniforms.lightDir); // Direction *to* the light
   
    // Get direct lighting
    vec3 Lo = pbrMetallicRoughness(albedo, metallic, roughness, N, V, L, 
                                 uniforms.lightColor, uniforms.lightIntensity);
    
    // Add simple ambient light
    vec3 ambient = vec3(0.05) * albedo;
    
    vec3 color = Lo + ambient;

    // --- 5. Post-processing (Tonemapping & Gamma) ---
    // PBR lighting is HDR, so we must map it back to LDR
    
    // Reinhard tonemapping
    color = color / (color + vec3(1.0));
    
    // Gamma correction (sRGB)
    color = pow(color, vec3(1.0/2.2)); 

	outColor = vec4(color, baseColor.a);
    if (outColor .a < 0.1)
	{
		discard;
	}
}