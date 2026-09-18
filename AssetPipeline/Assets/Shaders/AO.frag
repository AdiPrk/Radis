#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outAO;

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
    float radius;       
    int numSamples;     
    float scale;        
    float contrast;     
    int debugMode;
} pc;

const float PI = 3.14159265359;
const float SPIRAL_TURNS = 4.88692190558; 

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
    float depth = textureLod(gDepth, fragTexCoord, 0.0).r;
    if (depth >= 1.0) {
        outAO = vec4(1.0);
        return;
    }

    vec3 P = ReconstructWorldPos(fragTexCoord, depth);
    vec2 normalEnc = textureLod(gNormal, fragTexCoord, 0.0).rg;
    vec3 N = OctDecode(normalEnc);

    vec4 viewPos = uniforms.view * vec4(P, 1.0);
    float z_C = viewPos.z; 
    float d = max(-z_C, 0.0001); 

    ivec2 loc = ivec2(gl_FragCoord.xy);
    uint hash = ((30u * uint(loc.x)) ^ uint(loc.y)) + 10u * uint(loc.x) * uint(loc.y);
    float phi = float(hash);

    float R = pc.radius;
    float invRsq = 1.0 / (R * R); // Used for smooth falloff
    float s_float = float(pc.numSamples);
    float invSamples = 1.0 / s_float;
    
    // Algorithm constants
    float epsilon = 0.0001; 
    float beta = 0.001;     

    float biasThreshold = z_C * beta;
    vec2 uvScale = vec2(uniforms.projection[0][0], uniforms.projection[1][1]) * 0.5;
    float radiusToScreen = R / d; 

    float sum = 0.0;

    for (int i = 0; i < pc.numSamples; ++i) {
        float alpha = (float(i) + 0.5) * invSamples;
        float h = alpha * radiusToScreen;
        
        float theta = (float(i) + 0.5) * SPIRAL_TURNS + phi;

        vec2 offset = h * uvScale * vec2(cos(theta), sin(theta));
        vec2 sampleUV = fragTexCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

        float sampleDepth = textureLod(gDepth, sampleUV, 0.0).r;
        vec3 Pi = ReconstructWorldPos(sampleUV, sampleDepth);

        vec3 v_i = Pi - P; 
        float distSq = dot(v_i, v_i);

        // Smooth falloff instead of Heaviside step function
        float falloff = max(0.0, 1.0 - distSq * invRsq); 

        float numerator = max(0.0, dot(v_i, N) + biasThreshold) * falloff;
        float denominator = distSq + epsilon;

        sum += numerator / denominator;
    }

    float A = pow(max(0.0, 1.0 - (2.0 * pc.scale * invSamples) * sum), pc.contrast);

    outAO = vec4(A, A, A, 1.0);
}