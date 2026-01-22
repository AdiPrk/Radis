#pragma once

#include "ShaderTypes.h"
#include "UniformSettings.h"

#include "Graphics/Common/TextureLibrary.h" // For TextureLibrary::MAX_TEXTURE_COUNT

namespace Radis
{
    void CameraUniformInit(Uniform& uniform, RenderingResource& renderData);
    void RTUniformInit(Uniform& uniform, RenderingResource& renderData);
    void DeferredLightingUniformInit(Uniform& uniform, RenderingResource& renderData);
    void DeferredLightingUniformUpdate(Uniform& uniform, RenderingResource& renderData);

    // Called camera uniform but it's just everything until rhi is better set up
    inline VkShaderStageFlags rtFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                        VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                        VK_SHADER_STAGE_MISS_BIT_KHR |
                                        VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
                                        VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    const UniformSettings cameraUniformSettings = UniformSettings(CameraUniformInit)
        .AddUBBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags, sizeof(CameraUniforms)).SetDebugName("Camera Uniforms")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags, sizeof(InstanceUniforms), InstanceUniforms::MAX_INSTANCES).SetDebugName("Instance SSBO")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(VQS), 10000).SetDebugName("Animation SSBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags, TextureLibrary::MAX_TEXTURE_COUNT).SetDebugName("Texture SSBO")
        .AddSSBOBinding(VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags, sizeof(LightUniform) * 10000 + sizeof(uint32_t)).SetDebugName("Light SSBO");

    const UniformSettings rayTracingUniformSettings = UniformSettings(RTUniformInit)
        .AddASBinding(rtFlags, 1).SetDebugName("RT TLAS Buffer")
        .AddSSBIBinding(rtFlags, 1).SetDebugName("RT Color Image")
        .AddSSBIBinding(rtFlags, 1).SetDebugName("RT Heatmap Image")
        .AddSSBOBinding(rtFlags, sizeof(MeshDataUniform), 2000000).SetDebugName("RT Vertices SSBO")
        .AddSSBOBinding(rtFlags, sizeof(uint32_t), 10000000).SetDebugName("RT Indices SSBO");

    // Deferred lighting pass uniform settings
    // Binding 0: Camera UBO
    // Binding 1: G-Buffer Albedo (sampler2D)
    // Binding 2: G-Buffer Normal (sampler2D)
    // Binding 3: G-Buffer PBR (sampler2D)
    // Binding 4: G-Buffer Emissive (sampler2D)
    // Binding 5: G-Buffer Depth (sampler2D)
    // Binding 6: Light SSBO
    const UniformSettings deferredLightingUniformSettings = UniformSettings(DeferredLightingUniformInit)
        .AddUBBinding(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(CameraUniforms)).SetDebugName("Deferred Camera UBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Albedo")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Normal")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer PBR")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Emissive")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Depth")
        .AddSSBOBinding(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LightUniform) * 10000 + sizeof(uint32_t)).SetDebugName("Deferred Light SSBO");
}