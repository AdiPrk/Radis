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
        .AddSSBOBinding(rtFlags | compFlags, sizeof(MeshDataUniform), 15'000'000).SetDebugName("RT Vertices SSBO")
        .AddSSBOBinding(rtFlags | compFlags, sizeof(uint32_t), 30'000'000).SetDebugName("RT Indices SSBO")
        .AddISBinding(rtFlags | compFlags, 1).SetDebugName("RT History Read")
        .AddSSBIBinding(rtFlags | compFlags, 1).SetDebugName("RT History Write")
        .AddISBinding(rtFlags | compFlags, 1).SetDebugName("Environment Map");

    // Deferred lighting pass uniform
    const UniformSettings deferredLightingUniformSettings = UniformSettings({})
        .AddUBBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(CameraUniforms)).SetDebugName("Deferred Camera UBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "gAlbedo").SetDebugName("G-Buffer Albedo")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "gNormal").SetDebugName("G-Buffer Normal")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "gPBR").SetDebugName("G-Buffer PBR")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "gEmissive").SetDebugName("G-Buffer Emissive")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "SceneDepth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL).SetDebugName("G-Buffer Depth")
        .AddSSBOBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LightUniform) * LightUniform::MAX_LIGHTS + sizeof(uint32_t)).SetDebugName("Deferred Light SSBO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, Assets::ImagesPath + "Newport_Loft_Ref.hdr").SetDebugName("Environment Map")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, Assets::ImagesPath + "Newport_Loft_Ref.IRMAP.hdr").SetDebugName("Irradiance Map")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "BlurredAO").SetDebugName("SSAO Map");

    // Tone mapping pass - just reads the accumulated HDR texture
    const UniformSettings tonemapUniformSettings = UniformSettings({})
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "SceneHDR").SetDebugName("SceneHDR Texture");

    const UniformSettings alchemyAOUniformSettings = UniformSettings({})
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "SceneDepth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL).SetDebugName("SceneDepth")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "gNormal").SetDebugName("gNormal");

    const UniformSettings aoBlurHUniformSettings = UniformSettings({})
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "RawAO").SetDebugName("RawAO")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "SceneDepth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL).SetDebugName("SceneDepth")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "gNormal").SetDebugName("gNormal");

    const UniformSettings aoBlurVUniformSettings = UniformSettings({})
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "AOBlurTmp").SetDebugName("AOBlurTmp")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "SceneDepth", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL).SetDebugName("SceneDepth")
        .AddISBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1, "gNormal").SetDebugName("gNormal");
}