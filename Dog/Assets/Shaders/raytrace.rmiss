#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT HitPayload {
    vec3 color;
    float weight;
    int depth;
} payload;

void main()
{
    vec3 direction = normalize(gl_WorldRayDirectionEXT);

    // --- HARDCODED CONFIGURATION ---
    const vec3  UP_VECTOR       = vec3(0.0, 1.0, 0.0);
    const vec3  SUN_DIRECTION   = normalize(vec3(0.3, 0.8, 0.4)); // High afternoon sun
    
    const vec3  SKY_COLOR       = vec3(0.1, 0.4, 0.9);  // Deep Azure
    const vec3  HORIZON_COLOR   = vec3(0.8, 0.85, 0.9); // Hazy white/blue
    const vec3  GROUND_COLOR    = vec3(0.15, 0.13, 0.1); // Dark earth
    const vec3  SUN_COLOR       = vec3(1.0, 0.98, 0.85); // Warm sun

    // Tweak these for "look"
    const float HORIZON_SIZE    = 0.2;  // How soft the horizon blends
    const float SUN_SIZE        = 0.02; // Physical size of sun disk
    const float SUN_GLOW_SIZE   = 0.15; // Size of the halo
    const float SUN_SHARPNESS   = 2.0;  // Falloff of the glow
    const float SUN_INTENSITY   = 3.0;  // Brightness multiplier

    // --- SKY GRADIENT ---
    // Calculate elevation (angle from horizon)
    float elevation = asin(clamp(dot(direction, UP_VECTOR), -1.0, 1.0));
    
    // Blending factors for Ground -> Horizon -> Sky
    float top    = smoothstep(0.0, HORIZON_SIZE, elevation);
    float bottom = smoothstep(0.0, HORIZON_SIZE, -elevation);
    
    // Mix(mix(Horizon, Ground, bottom), Sky, top)
    vec3 environment = mix(mix(HORIZON_COLOR, GROUND_COLOR, bottom), SKY_COLOR, top);

    // --- SUN CALCULATION ---
    float angleToLight = acos(clamp(dot(direction, SUN_DIRECTION), 0.0, 1.0));
    float halfAngularSize = SUN_SIZE * 0.5;

    // Calculate sun glow/disk based on angle distance
    // Logic: 1.0 at center, fading out at (halfSize + glowSize)
    float glowInput = clamp(2.0 * (1.0 - smoothstep(halfAngularSize - SUN_GLOW_SIZE, 
                                                    halfAngularSize + SUN_GLOW_SIZE, 
                                                    angleToLight)), 0.0, 1.0);
    
    // Apply sharpness curve and intensity
    float glowIntensity = pow(glowInput, SUN_SHARPNESS) * SUN_INTENSITY;
    vec3 sunLight = SUN_COLOR * glowIntensity;

    // --- FINAL COMPOSITION ---
    payload.color = environment + sunLight;
}