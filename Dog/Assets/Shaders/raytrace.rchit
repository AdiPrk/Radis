#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT HitPayload {
    vec3 color;
    vec3 attenuation;
    int depth;
    vec3 nextRayOrigin;
    vec3 nextRayDir;    
    bool stop;    
} payload;

layout(location = 1) rayPayloadEXT bool isShadowed;

hitAttributeEXT vec2 attribs;

// --- Constants ---
const float PI = 3.14159265359;
const uint INVALID_TEXTURE_INDEX = 10001;

struct Instance
{
    mat4 model;
    vec4 tint;
    uvec4 textureIndicies;
    uvec4 textureIndicies2;
    vec4 baseColorFactor;
    vec4 metallicRoughnessFactor;
    vec4 emissiveFactor;
    uint boneOffset;
    uint indexOffset;
    uint vertexOffset;
};

layout(set = 0, binding = 1) readonly buffer InstanceData
{
    Instance instances[10000];
};

layout(set = 0, binding = 3) uniform sampler2D uTextures[];

struct Vertex
{
    vec3 position;
    vec3 color;   
    vec3 normal;  
    vec2 texCoord;
    // ivec4 boneIds;
    // vec4 weights; 
};

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
layout(set = 0, binding = 4) readonly buffer LightData {
    uint lightCount;
    Light lights[MAX_LIGHTS];
} lightData;

layout(set = 1, binding = 2, std430) readonly buffer MeshBuffer
{
    Vertex vertices[];
} meshBuffer;

layout(set = 1, binding = 3, std430) readonly buffer MeshIndexBuffer
{
    uint indices[];
} indexBuffer;

layout(set = 1, binding = 1) uniform accelerationStructureEXT topLevelAS;

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

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    // Adjusted Fresnel: Rough surfaces have less "flash" at grazing angles
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
    // vec3 F = FresnelSchlick(VdotH, F0);
    vec3 F = FresnelSchlickRoughness(VdotH, F0, roughness);
    
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * NdotL * lightColor;
}

// Helper to safely sample texture
vec4 SampleTexture(uint texIndex, vec2 uv)
{
    return texture(uTextures[texIndex], uv);
}

// Shadow Ray Function
float fetchShadow(vec3 worldPos, vec3 normal, vec3 lightDir, float lightDist)
{
    vec3 origin = worldPos + normal * 0.01; 
    isShadowed = true;

    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 1, origin, 0.001, lightDir, lightDist, 1);

    return isShadowed ? 0.0 : 1.0;
}

void main()
{
    Instance instance = instances[gl_InstanceCustomIndexEXT];

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    uvec3 tri = uvec3(gl_PrimitiveID * 3u + 0u, gl_PrimitiveID * 3u + 1u, gl_PrimitiveID * 3u + 2u);
    uint i0 = indexBuffer.indices[tri.x + instance.indexOffset];
    uint i1 = indexBuffer.indices[tri.y + instance.indexOffset];
    uint i2 = indexBuffer.indices[tri.z + instance.indexOffset];
    Vertex v0 = meshBuffer.vertices[i0 + instance.vertexOffset];
    Vertex v1 = meshBuffer.vertices[i1 + instance.vertexOffset];
    Vertex v2 = meshBuffer.vertices[i2 + instance.vertexOffset];

    vec2 uv = v0.texCoord * bary.x + v1.texCoord * bary.y + v2.texCoord * bary.z;
    vec3 normalLocal = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
    vec3 vertexColor = v0.color * bary.x + v1.color * bary.y + v2.color * bary.z;

    // Position: Origin + (Direction * Distance)
    vec3 fragWorldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    
    // Normal: Transform local normal to world
    mat3 normalMatrix = transpose(mat3(gl_WorldToObjectEXT));
    vec3 N = normalize(normalMatrix * normalLocal);
    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    float NdotV = max(dot(N, V), 0.0001); // Prevent divide by zero
    
    // Base Color
    vec4 baseColor = vec4(vertexColor, 1.0) * instance.tint * instance.baseColorFactor; // Included fragTint logic here
    if (instance.textureIndicies.x != INVALID_TEXTURE_INDEX)
    {
        baseColor *= SampleTexture(instance.textureIndicies.x, uv);
    }
    vec3 albedo = baseColor.rgb;

    // Metallic
    float metallic = instance.metallicRoughnessFactor.x;
    if (instance.textureIndicies.z != INVALID_TEXTURE_INDEX)
    {
        metallic = SampleTexture(instance.textureIndicies.z, uv).b;
    }

    // Roughness
    float roughness = instance.metallicRoughnessFactor.y;
    if (instance.textureIndicies.w != INVALID_TEXTURE_INDEX)
    {
        float rSample = SampleTexture(instance.textureIndicies.w, uv).g;
        roughness = roughness * rSample; // Generally multiplicative
    }
    roughness = clamp(roughness, 0.04, 1.0);

    // Ambient Occlusion
    float ao = 1.0;
    if (instance.textureIndicies2.x != INVALID_TEXTURE_INDEX)
    {
        ao = clamp(SampleTexture(instance.textureIndicies2.x, uv).r, 0.0, 1.0);        
    }

    // Emissive
    vec3 emissive = instance.emissiveFactor.rgb;
    if (instance.textureIndicies2.y != INVALID_TEXTURE_INDEX)
    {
        emissive *= SampleTexture(instance.textureIndicies2.y, uv).rgb;
    }

    // 6. Lighting Loop
    vec3 Lo = vec3(0.0);

    for (uint i = 0; i < lightData.lightCount; ++i)
    {
        Light light = lightData.lights[i];
        vec3 L;
        float attenuation = 1.0;
        float lightDistance = 0.0;

        if (light.type == 0) { // Directional
            L = normalize(-light.direction);
            lightDistance = 1e38; // Infinite distance for shadow ray
        }
        else { // Point / Spot
            vec3 toLight = light.position - fragWorldPos;
            lightDistance = length(toLight);
            float dist = length(toLight);
            L = normalize(toLight);
            attenuation = clamp(1.0 - dist / light.radius, 0.0, 1.0);
            attenuation *= attenuation;
        }

        if (light.type == 2) { // Spot
            float spotFactor = dot(L, -light.direction);
            float smoothS = smoothstep(light.outerCone, light.innerCone, spotFactor);
            attenuation *= smoothS;
        }

        // Shadow
        float NdotL = max(dot(N, L), 0.0);
        float shadowFactor = 1.0;

        if (NdotL > 0.0 && attenuation > 0.0)
        {
            shadowFactor = fetchShadow(fragWorldPos, N, L, lightDistance);
            vec3 lightCol = light.color * light.intensity * attenuation * shadowFactor;
            Lo += computePBRLight(albedo, metallic, roughness, N, V, L, lightCol);
        }
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    payload.color += (Lo * ao + ambient + emissive) * payload.attenuation;

    // --- NEXT RAY GENERATION (Reflections) ---
    payload.stop = true;
    
    // Threshold for stopping weak rays
    const float THRESHOLD = 0.01;
    vec3 throughput = payload.attenuation;
    float roughnessCutoff = 0.65;

    // CASE A: TRANSPARENT (Alpha handling)
    if (baseColor.a < 0.99) {
        // Simple transparency (no refraction)
        throughput *= (1.0 - baseColor.a);
        payload.nextRayDir = gl_WorldRayDirectionEXT;
        payload.nextRayOrigin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * (gl_HitTEXT + 0.001);
        payload.stop = false;
    }
    // CASE B: REFLECTION
    else if (metallic > 0.01 && roughness < roughnessCutoff) // Only reflect if somewhat shiny
    {
        // Fake roughness for reflection, Rougher = Darker Reflection.
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);

        float energyConservation = 1.0 - roughness;         
        energyConservation = energyConservation * energyConservation; 
        
        throughput *= F * energyConservation;

        payload.nextRayDir = reflect(gl_WorldRayDirectionEXT, N);
        payload.nextRayOrigin = fragWorldPos + N * 0.001;

        // If the throughput is super bright, clamp it.
        throughput = min(throughput, vec3(1.0));

        // Only continue if there is visible reflection energy left
        if (max(throughput.r, max(throughput.g, throughput.b)) > THRESHOLD) {
            payload.stop = false;
        }
    }

    payload.attenuation = throughput;
}