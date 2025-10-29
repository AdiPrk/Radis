#version 450

// VK Extensions
#ifdef VULKAN
	#extension GL_EXT_nonuniform_qualifier : require
#else
	#extension GL_ARB_bindless_texture : require
#endif

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec4 fragTint;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) flat in uint textureIndex;
layout(location = 5) flat in uint instanceIndex;

layout(location = 0) out vec4 outColor;

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
	layout(std430, binding = 3) readonly buffer TexHandles {
		uvec2 colorHandle[50];
	} texHandles;
#endif

void main()
{
#ifdef VULKAN
    if (textureIndex == 10001)
	{
		outColor = vec4(fragColor * fragTint.rgb, fragTint.a);
	}
	else
	{
		outColor = texture(uTextures[textureIndex], fragTexCoord) * fragTint;

		if (outColor.a < 0.1)
		{
			discard;
		}
	}
#else
	vec4 color = vec4(fragNormal.rgb * fragTint.rgb, fragTint.a);
    uvec2 colorH = texHandles.colorHandle[textureIndex];
	if (colorH != uvec2(0,0)) 
	{
		color = texture(sampler2D(colorH), fragTexCoord) * fragTint;
	}
	outColor = color;	
#endif
}