/*****************************************************************//**
 * \file   UniformData.h
 * \brief  Uniform buffer data initializations for Vulkan renderer.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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
    void TonemapUniformInit(Uniform& uniform, RenderingResource& renderData);
    void TonemapUniformUpdate(Uniform& uniform, RenderingResource& renderData);
    void ShadowMomentsUniformInit(Uniform& uniform, RenderingResource& renderData);
    void ShadowBlurUniformInitH(Uniform& uniform, RenderingResource& renderData);
    void ShadowBlurUniformInitV(Uniform& uniform, RenderingResource& renderData);

    // Called camera uniform but it's just everything until rhi is better set up
    inline VkShaderStageFlags rtFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                        VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                        VK_SHADER_STAGE_MISS_BIT_KHR |
                                        VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
                                        VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    inline VkShaderStageFlags compFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    const UniformSettings cameraUniformSettings = UniformSettings(CameraUniformInit)
        .AddUBBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags | compFlags, sizeof(CameraUniforms)).SetDebugName("Camera Uniforms")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags | compFlags, sizeof(InstanceUniforms), InstanceUniforms::MAX_INSTANCES).SetDebugName("Instance SSBO")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(VQS), 10000).SetDebugName("Animation SSBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags | compFlags, TextureLibrary::MAX_TEXTURE_COUNT).SetDebugName("Texture SSBO")
        .AddSSBOBinding(VK_SHADER_STAGE_FRAGMENT_BIT | rtFlags | compFlags, sizeof(LightUniform) * LightUniform::MAX_LIGHTS + sizeof(uint32_t)).SetDebugName("Light SSBO");

    const UniformSettings rayTracingUniformSettings = UniformSettings(RTUniformInit)
        .AddASBinding(rtFlags | compFlags, 1).SetDebugName("RT TLAS Buffer")
        .AddSSBIBinding(rtFlags | compFlags, 1).SetDebugName("RT Color Image")
        .AddSSBIBinding(rtFlags | compFlags, 1).SetDebugName("RT Heatmap Image")
        .AddSSBOBinding(rtFlags | compFlags, sizeof(MeshDataUniform), 10'000'000).SetDebugName("RT Vertices SSBO")
        .AddSSBOBinding(rtFlags | compFlags, sizeof(uint32_t), 30'000'000).SetDebugName("RT Indices SSBO")
        .AddISBinding(rtFlags | compFlags, 1).SetDebugName("RT History Read")
        .AddSSBIBinding(rtFlags | compFlags, 1).SetDebugName("RT History Write");

    // Deferred lighting pass uniform settings
    // Binding 0: Camera UBO
    // Binding 1-5: G-Buffer textures
    // Binding 6: Light SSBO (vertex + fragment for instanced light volumes)
    const UniformSettings deferredLightingUniformSettings = UniformSettings(DeferredLightingUniformInit)
        .AddUBBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(CameraUniforms)).SetDebugName("Deferred Camera UBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Albedo")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Normal")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer PBR")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Emissive")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("G-Buffer Depth")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LightUniform) * LightUniform::MAX_LIGHTS + sizeof(uint32_t)).SetDebugName("Deferred Light SSBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("ShadowMoments (blurred)")
        .AddUBBinding(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ShadowParamsUniform)).SetDebugName("Shadow Params UBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("Environment Map")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("Irradiance Map");

    // Tone mapping pass - just reads the accumulated HDR texture
    const UniformSettings tonemapUniformSettings = UniformSettings(TonemapUniformInit)
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1).SetDebugName("SceneHDR Texture");

    const UniformSettings shadowMomentsUniformSettings = UniformSettings(ShadowMomentsUniformInit)
        .AddUBBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ShadowCameraUniform)).SetDebugName("Shadow Moments UBO")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(InstanceUniforms), InstanceUniforms::MAX_INSTANCES).SetDebugName("Shadow Instance SSBO")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT, sizeof(VQS), 10000).SetDebugName("Shadow Bone SSBO");

    const UniformSettings shadowBlurHUniformSettings = UniformSettings(ShadowBlurUniformInitH)
        .AddISBinding(VK_SHADER_STAGE_COMPUTE_BIT, 1).SetDebugName("Blur Src (sampled)")
        .AddSSBIBinding(VK_SHADER_STAGE_COMPUTE_BIT, 1).SetDebugName("Blur Dst (storage image)");

    const UniformSettings shadowBlurVUniformSettings = UniformSettings(ShadowBlurUniformInitV)
        .AddISBinding(VK_SHADER_STAGE_COMPUTE_BIT, 1).SetDebugName("Blur Src (sampled)")
        .AddSSBIBinding(VK_SHADER_STAGE_COMPUTE_BIT, 1).SetDebugName("Blur Dst (storage image)");
}