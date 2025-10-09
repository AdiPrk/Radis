#pragma once

#include "ShaderTypes.h"
#include "UniformSettings.h"

#include "../Texture/TextureLibrary.h" // For TextureLibrary::MAX_TEXTURE_COUNT

namespace Dog
{
    void CameraUniformInit(Uniform& uniform, RenderingResource& renderData);
    void InstanceUniformInit(Uniform& uniform, RenderingResource& renderData);

    const UniformSettings cameraUniformSettings = UniformSettings(CameraUniformInit)
        .AddUBBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(CameraUniforms));

    const UniformSettings instanceUniformSettings = UniformSettings(InstanceUniformInit)
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, TextureLibrary::MAX_TEXTURE_COUNT)
        .AddVertexBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(InstanceUniforms), InstanceUniforms::MAX_INSTANCES)
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(AnimationUniforms), AnimationUniforms::MAX_BONES);

}