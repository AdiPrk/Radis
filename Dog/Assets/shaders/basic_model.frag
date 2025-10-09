#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec4 fragTint;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) flat in uint textureIndex;
layout(location = 5) flat in uint instanceIndex;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uTextures[]; 

struct Instance {
  mat4 model;
  vec4 tint;
  uint textureIndex;
  uint boneOffset;
};

layout(std430, set = 1, binding = 1) buffer readonly InstanceDataBuffer {
  Instance instances[10000];
} instanceData;

void main()
{
	if (textureIndex == 10001)
	{
		outColor = vec4(fragColor * fragTint.rgb, fragTint.a);
	}
	else
	{
		outColor = texture(uTextures[textureIndex], fragTexCoord);
		outColor *= fragTint;

		if (outColor.a < 0.1)
		{
			discard;
		}
	}
}