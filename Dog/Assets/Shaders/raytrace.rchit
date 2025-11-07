#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT HitPayload {
    vec3 color;
    float weight;
    int depth;
} payload;

hitAttributeEXT vec2 attribs;

// --- Helper functions to turn an ID into a color ---
uint hash(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

vec3 hashToColor(uint x) {
    uint h = hash(x);
    float r = float(h & 0xFF) / 255.0;
    float g = float((h >> 8) & 0xFF) / 255.0;
    float b = float((h >> 16) & 0xFF) / 255.0;
    return vec3(r, g, b);
}
// --------------------------------------------------

void main()
{
    payload.color = hashToColor(gl_InstanceCustomIndexEXT);
}