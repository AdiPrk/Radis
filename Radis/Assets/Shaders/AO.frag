#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outAO; // Single channel output for the AO factor

// ---------------------------------------------------------------------------
// Uniforms & Bindings
// ---------------------------------------------------------------------------
#ifdef VULKAN
    #define UBO_LAYOUT(s, b) layout(set = s, binding = b)
#else
    #define UBO_LAYOUT(s, b) layout(std140, binding = b)
#endif

UBO_LAYOUT(0, 0) uniform Uniforms {
    mat4 projectionView;
    mat4 projection;
    mat4 view;
    mat4 invProjView;
    vec3 cameraPos;
} uniforms;

layout(set = 1, binding = 0) uniform sampler2D gDepth;
layout(set = 1, binding = 1) uniform sampler2D gNormal;

layout(push_constant) uniform AOPushConstants {
    float radius;       // R: Range of influence (e.g., 1.0)
    int numSamples;     // n: Number of samples (e.g., 10 to 20)
    float scale;        // s: Contrast scale
    float contrast;     // k: Contrast exponent
    int debugMode;
} pc;

const float PI = 3.14159265359;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main() {
    float depth = texture(gDepth, fragTexCoord).r;
    if (depth >= 1.0) {
        outAO = vec4(1.0); // No occlusion on the skybox
        return;
    }

    vec3 P = ReconstructWorldPos(fragTexCoord, depth);
    vec2 normalEnc = texture(gNormal, fragTexCoord).rg;
    vec3 N = OctDecode(normalEnc);

    // Camera space depth for spiral scaling
    vec4 viewPos = uniforms.view * vec4(P, 1.0);
    float d = max(-viewPos.z, 0.0001); // Avoid division by zero just in case

    // Pseudo-random rotation hash based on integer pixel coordinates
    ivec2 loc = ivec2(gl_FragCoord.xy);
    uint hash = ((30u * uint(loc.x)) ^ uint(loc.y)) + 10u * uint(loc.x) * uint(loc.y);
    float phi = float(hash); 

    float R = pc.radius;
    float c = 0.1 * R;
    float delta = 0.001;
    float n_float = float(pc.numSamples);

    vec2 uvScale = vec2(uniforms.projection[0][0], uniforms.projection[1][1]) * 0.5;

    float sum = 0.0;

    for (int i = 0; i < pc.numSamples; ++i) {
        float alpha = (float(i) + 0.5) / n_float;
        
        // Base ratio
        float h = alpha * R / d;
        // Spiral angle: 7 turns for every 9 points
        float theta = 2.0 * PI * alpha * (7.0 * n_float / 9.0) + phi; 

        vec2 offset = h * uvScale * vec2(cos(theta), sin(theta));
        vec2 sampleUV = fragTexCoord + offset;

        // Clamp to screen bounds
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

        float sampleDepth = texture(gDepth, sampleUV).r;
        vec3 Pi = ReconstructWorldPos(sampleUV, sampleDepth);

        vec3 omega = Pi - P;
        float distSq = dot(omega, omega);

        // Heaviside step function: 1 if within range R, 0 otherwise
        float H = (sqrt(distSq) < R) ? 1.0 : 0.0;

        // Depth of Pi for the delta check
        vec4 viewPi = uniforms.view * vec4(Pi, 1.0);
        float di = -viewPi.z;

        float numerator = max(0.0, dot(N, omega) - delta * di) * H;
        float denominator = max(c * c, distSq);

        sum += numerator / denominator;
    }

    float S = (2.0 * PI * c / n_float) * sum;
    
    float A = pow(max(0.0, 1.0 - pc.scale * S), pc.contrast);

    outAO = vec4(A, A, A, 1.0);
}