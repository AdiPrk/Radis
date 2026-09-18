#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b)
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
#endif

// Camera Uniforms
UBO_LAYOUT(0, 0) uniform Uniforms {
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
} uniforms;

// Texture Bindings
layout(set = 1, binding = 0) uniform sampler2D inputTexture;
layout(set = 1, binding = 1) uniform sampler2D gDepth;
layout(set = 1, binding = 2) uniform sampler2D gNormal;

layout(push_constant) uniform BlurPushConstants {
    vec2 direction;     // (1/width, 0) for Horizontal, (0, 1/height) for Vertical
    int radius;         // Blur radius
    float spatialSigma; // Spread of the spatial blur
    float rangeSigma;   // spread of the depth awareness
} pc;

vec3 ReconstructWorldPos(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
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

void main() {
    float centerDepth = texture(gDepth, fragTexCoord).r;
    if (centerDepth >= 1.0) {
        outColor = vec4(1.0); // Skybox
        return;
    }

    vec3 centerNormal = OctDecode(texture(gNormal, fragTexCoord).rg);
    vec3 centerWorld = ReconstructWorldPos(fragTexCoord, centerDepth);
    float d = -(uniforms.view * vec4(centerWorld, 1.0)).z;

    float sumAO = 0.0;
    float sumWeight = 0.0;

    float spatialDenom = 2.0 * pc.spatialSigma * pc.spatialSigma;
    float rangeDenom = 2.0 * pc.rangeSigma;

    for (int i = -pc.radius; i <= pc.radius; ++i) {
        vec2 sampleUV = fragTexCoord + pc.direction * float(i);
        
        // Clamp to screen edges
        sampleUV = clamp(sampleUV, 0.0, 1.0);

        float sampleDepth = texture(gDepth, sampleUV).r;
        if (sampleDepth >= 1.0) continue; 

        vec3 sampleNormal = OctDecode(texture(gNormal, sampleUV).rg);
        vec3 sampleWorld = ReconstructWorldPos(sampleUV, sampleDepth);
        float d_i = -(uniforms.view * vec4(sampleWorld, 1.0)).z; 

        // Spatial Kernel
        float spatialWeight = exp(-(float(i * i)) / spatialDenom);

        // Range Kernel
        float NdotN = max(0.0, dot(sampleNormal, centerNormal));
        float depthDiff = d_i - d;
        float rangeWeight = NdotN * exp(-(depthDiff * depthDiff) / rangeDenom);

        // Combine weights
        float weight = spatialWeight * rangeWeight;

        sumAO += texture(inputTexture, sampleUV).r * weight;
        sumWeight += weight;
    }

    // Normalize and output
    float blurredAO = sumAO / (sumWeight + 0.0001); // Add epsilon to prevent div by 0
    outColor = vec4(blurredAO, blurredAO, blurredAO, 1.0);
}