#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
const float PI           = 3.14159265359;
const float INV_PI       = 0.31830988618;
const float TWO_PI       = 6.28318530718;
const float HALF_PI      = 1.57079632679;
const float EPSILON      = 1e-7;
const vec3  F0_NON_METAL = vec3(0.04);

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b)
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
#endif

// ---------------------------------------------------------------------------
// Uniforms & Buffers
// ---------------------------------------------------------------------------
UBO_LAYOUT(0, 0) uniform Uniforms {
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

layout(set = 0, binding = 7)  uniform sampler2D envMap;
layout(set = 0, binding = 8) uniform sampler2D irradianceMap;
layout(set = 0, binding = 9) uniform sampler2D ssaoMap;

layout(push_constant) uniform PushConstants {
    int useIrrDiffuse;
    int specTestMode; // 0 = Final, 1 = Mirror, 2 = Ghosting, 3 = Monte Carlo
};

// ---------------------------------------------------------------------------
// Hammersley Sequence (40 Samples)
// ---------------------------------------------------------------------------
const uint SAMPLE_COUNT = 40;
const vec2 hammersley[40] = vec2[40](
    vec2(0.000000, 0.012500), vec2(0.500000, 0.037500), vec2(0.250000, 0.062500), vec2(0.750000, 0.087500),
    vec2(0.125000, 0.112500), vec2(0.625000, 0.137500), vec2(0.375000, 0.162500), vec2(0.875000, 0.187500),
    vec2(0.062500, 0.212500), vec2(0.562500, 0.237500), vec2(0.312500, 0.262500), vec2(0.812500, 0.287500),
    vec2(0.187500, 0.312500), vec2(0.687500, 0.337500), vec2(0.437500, 0.362500), vec2(0.937500, 0.387500),
    vec2(0.031250, 0.412500), vec2(0.531250, 0.437500), vec2(0.281250, 0.462500), vec2(0.781250, 0.487500),
    vec2(0.156250, 0.512500), vec2(0.656250, 0.537500), vec2(0.406250, 0.562500), vec2(0.906250, 0.587500),
    vec2(0.093750, 0.612500), vec2(0.593750, 0.637500), vec2(0.343750, 0.662500), vec2(0.843750, 0.687500),
    vec2(0.218750, 0.712500), vec2(0.718750, 0.737500), vec2(0.468750, 0.762500), vec2(0.968750, 0.787500),
    vec2(0.015625, 0.812500), vec2(0.515625, 0.837500), vec2(0.265625, 0.862500), vec2(0.765625, 0.887500),
    vec2(0.140625, 0.912500), vec2(0.640625, 0.937500), vec2(0.390625, 0.962500), vec2(0.890625, 0.987500)
);

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
vec3 ReconstructWorldPos(vec2 uv, float depth) {
    vec4 clipPos  = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 worldPos = uniforms.invProjView * clipPos;
    return worldPos.xyz / worldPos.w;
}

vec3 OctDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.xy += mix(vec2(t), vec2(-t), greaterThanEqual(n.xy, vec2(0.0)));
    return normalize(n);
}

vec3 GetSkyDirection(vec2 uv) {
    vec4 ndc      = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldPos = uniforms.invProjView * ndc;
    return normalize(worldPos.xyz / worldPos.w - uniforms.cameraPos);
}

vec2 DirToEquirect(vec3 dir) {
    const vec2 invAtan = vec2(0.1591, 0.3183); // (1/2pi, 1/pi)
    vec2 uv = vec2(atan(dir.z, dir.x), asin(clamp(dir.y, -1.0, 1.0)));
    return (uv * invAtan) + 0.5;
}

vec2 uvOf(vec3 dir) {
    return vec2(
        0.5 - atan(dir.z, dir.x) * (0.5 * INV_PI),
        asin(clamp(dir.y, -1.0, 1.0)) * INV_PI + 0.5
    );
}

// ---------------------------------------------------------------------------
// PBR helpers
// ---------------------------------------------------------------------------
float D_GGX(float NdotH, float a2) {
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + EPSILON);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2) {
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL + EPSILON);
}

vec3 F_Schlick(float VdotH, vec3 F0) {
    float f  = 1.0 - VdotH;
    float f2 = f * f;
    return F0 + (1.0 - F0) * (f2 * f2 * f);
}

// ---------------------------------------------------------------------------
// Lighting Calculations
// ---------------------------------------------------------------------------
vec3 computePBRLight(vec3 albedo, float metallic, vec3 N, vec3 V, vec3 L, vec3 lightColor, float a2, vec3 F0, float NdotV)
{
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float D   = D_GGX(NdotH, a2);
    float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a2);
    vec3  F   = F_Schlick(VdotH, F0);

    vec3 specular = D * Vis * F;
    vec3 kD       = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse  = kD * albedo * INV_PI;

    return (diffuse + specular) * NdotL * lightColor;
}

vec3 computeIBLDiffuse(vec3 albedo, float metallic, float ao, vec3 N, vec3 F0)
{
    vec3 kD = (1.0 - F0) * (1.0 - metallic);
    vec3 irradiance = texture(irradianceMap, uvOf(N)).rgb;
    return kD * albedo * INV_PI * irradiance * ao;
}

vec3 computeIBLSpecular(vec3 N, vec3 V, float a, float a2, vec3 F0, float NdotV)
{
    vec3 R = reflect(-V, N);

    // Branchless basis vector selection
    vec3 up = vec3(1.0, 0.0, 0.0);
    up = mix(up, vec3(0.0, 0.0, 1.0), step(abs(R.z), 0.999));
    vec3 A = normalize(cross(up, R));
    vec3 B = normalize(cross(R, A));

    vec3 specularSum = vec3(0.0);
    vec2 envSize = vec2(textureSize(envMap, 0));

    // --- TEST HARNESS OVERRIDES ---
    uint activeSamples = SAMPLE_COUNT;
    bool forceLodZero = false;

    if (specTestMode == 1) { activeSamples = 1; forceLodZero = true; }
    else if (specTestMode == 2) { activeSamples = 3; forceLodZero = true; }
    else if (specTestMode == 3) { activeSamples = SAMPLE_COUNT; forceLodZero = true; }
    // ------------------------------

    float resolutionScale = 0.5 * log2(envSize.x * envSize.y / float(activeSamples));

    for (uint i = 0; i < activeSamples; ++i)
    {
        vec2 xi = hammersley[i];
        float xi1 = xi.x;
        float xi2 = xi.y;

        // Fast Trig Identities for GGX
        float phi = TWO_PI * (0.5 - xi1);
        // float cosTheta = sqrt((1.0 - xi2) / (1.0 + (a2 - 1.0) * xi2));
        // float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        float cosTheta = sqrt(clamp((1.0 - xi2) / (1.0 + (a2 - 1.0) * xi2), 0.0, 1.0));
        float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

        // Spherical to Cartesian (Local D vector)
        vec3 D = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

        // Transform D to world space centered around R
        vec3 wk = normalize(D.x * A + D.y * B + D.z * R);

        float NdotL = max(dot(N, wk), 0.0);
        
        if (NdotL > 0.0)
        {
            vec3 H = normalize(V + wk);
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);

            // Calculate appropriate Mipmap Level
            //float D_val = D_GGX(NdotH, a2);
            //float level = max(0.0, resolutionScale - 0.5 * log2(max(D_val, 1e-5) / 4.0));
            float a2_mip = max(a2, 1e-4);
            float D_val  = D_GGX(NdotH, a2_mip);   // for level only
            float level  = max(0.0, resolutionScale - 0.5 * log2(D_val / 4.0));
            if (forceLodZero) { level = 0.0; } 

            // Sample Environment Map
            vec3 Li = textureLod(envMap, uvOf(wk), level).rgb;

            // Evaluate BRDF terms
            float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a2);
            vec3 F = F_Schlick(VdotH, F0);

            specularSum += Li * Vis * F * NdotL;
        }
    }

    return specularSum / float(activeSamples);
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
        outColor = vec4(vec3(0.0), 1.0);
        return;
    }

    // G-Buffer unpack
    vec4 albedoSample = texture(gAlbedo, fragTexCoord);
    vec2 normalEnc    = texture(gNormal, fragTexCoord).rg;
    vec4 pbrSample    = texture(gPBR,    fragTexCoord);
    vec3 emissive     = texture(gEmissive, fragTexCoord).rgb;

    vec3  albedo    = albedoSample.rgb;
    vec3  N         = OctDecode(normalEnc);
    float metallic  = pbrSample.r;
    float roughness = max(pbrSample.g, 0.05);
    //float ao        = pbrSample.b;
    float materialAO = pbrSample.b;
    // float screenSpaceAO = texture(ssaoMap, fragTexCoord).r;
    float ao = materialAO; // * */screenSpaceAO; // Combine baked and dynamic AO!

    vec3 worldPos = ReconstructWorldPos(fragTexCoord, depth);
    vec3 V        = normalize(uniforms.cameraPos - worldPos);

    // --- HOISTED PER-PIXEL MATH ---
    // Calculated once per pixel instead of inside every light/IBL function
    float a = roughness * roughness;
    float a2 = a * a;
    vec3 F0 = mix(F0_NON_METAL, albedo, metallic);
    float NdotV = max(dot(N, V), EPSILON);
    // ------------------------------

    // Direct lighting
    vec3 Lo = vec3(0.0);
    uint lightCount = lightData.lightCount;
    for (uint i = 0; i < lightCount; ++i)
    {
        Light light = lightData.lights[i];
        if (light.outerConeType.y != 0.0) break; // directional lights only

        vec3 L = normalize(-light.directionInner.xyz);
        vec3 lightCol = light.colorIntensity.xyz * light.colorIntensity.w;

        Lo += computePBRLight(albedo, metallic, N, V, L, lightCol, a2, F0, NdotV);
    }

    // IBL Modes
    if (useIrrDiffuse == 0) // Regular lighting
    {
        vec3 iblDiffuse = computeIBLDiffuse(albedo, metallic, ao, N, F0);
        vec3 iblSpecular = computeIBLSpecular(N, V, a, a2, F0, NdotV);

        vec3 finalColor = Lo + iblDiffuse + (iblSpecular * ao) + emissive;
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
        vec3 iblDiffuse = computeIBLDiffuse(albedo, metallic, ao, N, F0);
        vec3 finalColor = Lo + iblDiffuse + emissive;
        
        if (fragTexCoord.x > 0.5) {
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