#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT HitPayload {
    vec3 color;
    float weight;
    int depth;
} payload;

hitAttributeEXT vec2 attribs;

void main()
{
    // Simple solid red for all hits
    payload.color = vec3(1.0, 0.0, 0.0);
}