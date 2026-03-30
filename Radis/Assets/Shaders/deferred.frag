#version 460
#ifdef VULKAN
    #extension GL_EXT_nonuniform_qualifier : require
#else
    #extension GL_ARB_bindless_texture : require
#endif

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec4 fragTint;
layout(location = 2) in vec3 fragWorldNormal;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) flat in uvec4 textureIndices;
layout(location = 5) flat in uvec4 textureIndices2;
layout(location = 6) flat in vec4 baseColorFactor;
layout(location = 7) flat in vec4 metallicRoughnessFactor;
layout(location = 8) flat in vec4 emissiveFactor;
layout(location = 9) flat in uint instanceIndex;
layout(location = 10) in vec3 fragWorldPos;
layout(location = 11) in vec3 localPos;

// G-Buffer outputs (MRT)
layout(location = 0) out vec4 gAlbedo;    // R8G8B8A8_SRGB  - RGB = albedo, A = alpha
layout(location = 1) out vec2 gNormal;    // R16G16_SFLOAT  - Octahedral encoded normal
layout(location = 2) out vec4 gPBR;       // R8G8B8A8_UNORM - R = metallic, G = roughness, B = AO, A = unused
layout(location = 3) out vec3 gEmissive;  // B10G11R11_UFLOAT_PACK32 - HDR emissive

const uint INVALID_TEXTURE_INDEX = 10001;

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b)
    #define SSBO_LAYOUT(s, b) layout(set = s, binding = b)
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
    #define SSBO_LAYOUT(s, b) layout(std430, binding = b)
#endif

#ifdef VULKAN
    layout(set = 0, binding = 3) uniform sampler2D uTextures[];
#else
    SSBO_LAYOUT(0, 3) readonly buffer TexHandles {
        uvec2 colorHandle[];
    } texHandles;
#endif

// Texture sampling helper
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

// Octahedral normal encoding
// Encodes a unit normal vector to a 2D octahedral representation
vec2 OctEncode(vec3 n)
{
    // Project to octahedron
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    
    // Wrap the bottom hemisphere
    if (n.z < 0.0)
    {
        vec2 wrapped = (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
        n.xy = wrapped;
    }
    
    // Map from [-1, 1] to [0, 1] for storage
    return n.xy * 0.5 + 0.5;
}

void main()
{
    // --- Albedo ---
    vec4 baseColor = vec4(fragColor * fragTint.rgb, fragTint.a) * baseColorFactor;
    if (textureIndices.x != INVALID_TEXTURE_INDEX)
    {
        baseColor = SampleTexture(textureIndices.x, fragTexCoord) * fragTint * baseColorFactor;
    }
    
    // Alpha test - discard transparent fragments
    if (baseColor.a < 0.1)
    {
        discard;
    }

    // --- Metallic ---
    float metallic = metallicRoughnessFactor.x;
    if (textureIndices.z != INVALID_TEXTURE_INDEX)
    {
        metallic = SampleTexture(textureIndices.z, fragTexCoord).b;
    }

    // --- Roughness ---
    float roughness = metallicRoughnessFactor.y;
    if (textureIndices.w != INVALID_TEXTURE_INDEX)
    {
        roughness = SampleTexture(textureIndices.w, fragTexCoord).g;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    // --- Normal ---
    vec3 N = normalize(fragWorldNormal);
    // TODO: Normal mapping support would go here

    // --- Ambient Occlusion ---
    float ao = 1.0;
    if (textureIndices2.x != INVALID_TEXTURE_INDEX)
    {
        ao = clamp(SampleTexture(textureIndices2.x, fragTexCoord).r, 0.0, 1.0);
    }

    // --- Emissive (HDR) ---
    vec3 emissive = emissiveFactor.rgb;
    if (textureIndices2.y != INVALID_TEXTURE_INDEX)
    {
        emissive *= SampleTexture(textureIndices2.y, fragTexCoord).rgb;
    }

    // --- Write to G-Buffer ---
    gAlbedo = vec4(baseColor.rgb, baseColor.a);
    gNormal = OctEncode(N);                        // Octahedral encoded normal (R16G16)
    gPBR = vec4(metallic, roughness, ao, 1.0);
    gEmissive = emissive;                          // HDR emissive (B10G11R11)
}