#pragma once

#include "ShaderTypes.h"
#include "UniformSettings.h"

#include "../Texture/TextureLibrary.h" // For TextureLibrary::MAX_TEXTURE_COUNT

namespace Dog
{
    void CameraUniformInit(Uniform& uniform, RenderingResource& renderData);
    //void InstanceUniformInit(Uniform& uniform, RenderingResource& renderData);

    // Called camera uniform but it's just everything until rhi is better set up
    const UniformSettings cameraUniformSettings = UniformSettings(CameraUniformInit)
        .AddUBBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(CameraUniforms))
        .AddVertexBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), SimpleInstanceUniforms::MAX_INSTANCES);

    //const UniformSettings instanceUniformSettings = UniformSettings(InstanceUniformInit)
    //    .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, TextureLibrary::MAX_TEXTURE_COUNT)
    //    .AddVertexBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(InstanceUniforms), InstanceUniforms::MAX_INSTANCES)
    //    .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(AnimationUniforms), AnimationUniforms::MAX_BONES);

}