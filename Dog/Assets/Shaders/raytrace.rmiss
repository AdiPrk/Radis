#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT HitPayload {
    vec3 color;
    float weight;
    int depth;
} payload;

void main()
{
    // Background color
    payload.color = vec3(0.5);
}
