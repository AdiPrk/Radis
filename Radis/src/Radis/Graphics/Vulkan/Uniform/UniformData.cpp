/*****************************************************************//**
 * \file   UniformData.cpp
 * \brief  Implementation of uniform data initialization for Vulkan rendering.
 * 
 * \author Aditya Prakash
 * \date   February 2026
 *********************************************************************/

#include <PCH/pch.h>

#include "UniformData.h"
#include "Uniform.h"
#include "Descriptors.h"

#include "ECS/Resources/RenderingResource.h"
#include "../Core/Buffer.h"
#include "../Core/SwapChain.h"

#include "../Texture/VKTexture.h"

namespace Radis
{
    void CameraUniformInit(Uniform& uniform, RenderingResource& renderData)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();
        size_t textureCount = renderData.textureLibrary->GetTextureCount();

        std::vector<VkDescriptorImageInfo> imageInfos(TextureLibrary::MAX_TEXTURE_COUNT);
        for (size_t j = 0; j < TextureLibrary::MAX_TEXTURE_COUNT; ++j) 
        {
            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[j].sampler = defaultSampler;

            if (textureCount == 0) 
            {
                imageInfos[j].imageView = VK_NULL_HANDLE;
            }
            else
            {
                ITexture* itex = renderData.textureLibrary->GetTextureByIndex(static_cast<uint32_t>(std::min(j, textureCount - 1)));
                VKTexture* vktex = static_cast<VKTexture*>(itex);
                if (vktex)
                {
                    imageInfos[j].imageView = vktex->GetImageView();
                }
            }
        }

        // Build descriptor sets for each frame with both buffer and texture data
        for(int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            // Bind uniform buffers (0-2) directly
            const Buffer& ubuf0 = uniform.GetUniformBuffer(0, frameIndex);
            const Buffer& ubuf1 = uniform.GetUniformBuffer(1, frameIndex);
            const Buffer& ubuf2 = uniform.GetUniformBuffer(2, frameIndex);
            const Buffer& ubuf4 = uniform.GetUniformBuffer(4, frameIndex);

            VkDescriptorBufferInfo bufferInfo0{
                .buffer = ubuf0.buffer,
                .range = ubuf0.bufferSize
            };
            VkDescriptorBufferInfo bufferInfo1{
                .buffer = ubuf1.buffer,
                .range = ubuf1.bufferSize
            };
            VkDescriptorBufferInfo bufferInfo2{
                .buffer = ubuf2.buffer,
                .range = ubuf2.bufferSize
            };
            VkDescriptorBufferInfo bufferInfo4{
                .buffer = ubuf4.buffer,
                .range = ubuf4.bufferSize
            };

            writer.WriteBuffer(0, &bufferInfo0);
            writer.WriteBuffer(1, &bufferInfo1);
            writer.WriteBuffer(2, &bufferInfo2);
            writer.WriteImage(3, imageInfos.data(), static_cast<uint32_t>(imageInfos.size()));
            writer.WriteBuffer(4, &bufferInfo4);

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void RTUniformInit(Uniform& uniform, RenderingResource& renderData)
    {
        uint32_t width = renderData.swapChain->GetSwapChainExtent().width;
        uint32_t height = renderData.swapChain->GetSwapChainExtent().height;
        VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<VKTexture*> rtTextures(SwapChain::MAX_FRAMES_IN_FLIGHT * 2);

        // 1. Create Ping-Pong Accumulation Textures (Notice the name change to RTAccum)
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i) {
            std::string texName = "RTAccum_" + std::to_string(i);
            uint32_t rtInd = renderData.textureLibrary->CreateStorageImage(texName, width, height, format, usage, layout);
            rtTextures[i] = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture(rtInd));
        }
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i) {
            std::string texName = "RTHeatmapImage_" + std::to_string(i);
            uint32_t rtInd = renderData.textureLibrary->CreateStorageImage(texName, width, height, format, usage, layout);
            rtTextures[i + SwapChain::MAX_FRAMES_IN_FLIGHT] = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture(rtInd));
        }

        // 2. Fetch SceneHDR (The bridge to the Tonemapper)
        VKTexture* sceneHDRTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("SceneHDR"));
        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();

        VKTexture* envMapTex = static_cast<VKTexture*>(
            renderData.textureLibrary->GetTextureByIndex(renderData.envMapIndex)
            );

        if (!envMapTex)
        {
            RADIS_ERROR("Environment map texture not found!");
            return;
        }

        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            int historyIndex = (frameIndex + SwapChain::MAX_FRAMES_IN_FLIGHT - 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;

            VkDescriptorImageInfo outSceneHDRInfo{ VK_NULL_HANDLE, sceneHDRTex->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };
            VkDescriptorImageInfo heatmapInfo{ VK_NULL_HANDLE, rtTextures[frameIndex + SwapChain::MAX_FRAMES_IN_FLIGHT]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };

            // HISTORY READ: Bound as a Sampler! RenderGraph will safely transition this to READ_ONLY_OPTIMAL
            VkDescriptorImageInfo historyReadInfo{ defaultSampler, rtTextures[historyIndex]->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

            // HISTORY WRITE: Bound as a Storage Image (GENERAL layout)
            VkDescriptorImageInfo historyWriteInfo{ VK_NULL_HANDLE, rtTextures[frameIndex]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };

            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            const Buffer& ubuf2 = uniform.GetUniformBuffer(3, frameIndex);
            const Buffer& ubuf3 = uniform.GetUniformBuffer(4, frameIndex);
            VkDescriptorBufferInfo bufferInfo2{ .buffer = ubuf2.buffer, .range = ubuf2.bufferSize };
            VkDescriptorBufferInfo bufferInfo3{ .buffer = ubuf3.buffer, .range = ubuf3.bufferSize };

            VkDescriptorImageInfo envMapInfo{};
            envMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            envMapInfo.sampler = defaultSampler;
            envMapInfo.imageView = envMapTex->GetImageView();

            writer.WriteImage(1, &outSceneHDRInfo);
            writer.WriteImage(2, &heatmapInfo);
            writer.WriteBuffer(3, &bufferInfo2);
            writer.WriteBuffer(4, &bufferInfo3);
            writer.WriteImage(5, &historyReadInfo);
            writer.WriteImage(6, &historyWriteInfo);
            writer.WriteImage(7, &envMapInfo);

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void DeferredLightingUniformInit(Uniform& uniform, RenderingResource& renderData)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();

        // Get G-Buffer textures
        VKTexture* gAlbedoTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gAlbedo"));
        VKTexture* gNormalTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gNormal"));
        VKTexture* gPBRTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gPBR"));
        VKTexture* gEmissiveTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gEmissive"));
        VKTexture* gDepthTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("SceneDepth"));
        VKTexture* shadowMomentsTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("ShadowMoments"));
        VKTexture* shadowDepthTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("ShadowDepth"));

        VKTexture* envMapTex = static_cast<VKTexture*>(
            renderData.textureLibrary->GetTextureByIndex(renderData.envMapIndex)
        );
        VKTexture* irMapTex = static_cast<VKTexture*>(
            renderData.textureLibrary->GetTextureByIndex(renderData.irMapIndex)
        );

        if (!gAlbedoTex || !gNormalTex || !gPBRTex || !gEmissiveTex || !gDepthTex || !shadowMomentsTex || !shadowDepthTex || !envMapTex || !irMapTex)
        {
            RADIS_ERROR("One or more textures not found!");
            return;
        }

        // Build descriptor sets for each frame
        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            // Binding 0: Camera UBO
            const Buffer& cameraBuffer = uniform.GetUniformBuffer(0, frameIndex);
            VkDescriptorBufferInfo cameraBufferInfo{
                .buffer = cameraBuffer.buffer,
                .offset = 0,
                .range = cameraBuffer.bufferSize
            };
            writer.WriteBuffer(0, &cameraBufferInfo);

            // Binding 1: G-Buffer Albedo
            VkDescriptorImageInfo albedoImageInfo{
                .sampler = defaultSampler,
                .imageView = gAlbedoTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(1, &albedoImageInfo);

            // Binding 2: G-Buffer Normal
            VkDescriptorImageInfo normalImageInfo{
                .sampler = defaultSampler,
                .imageView = gNormalTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(2, &normalImageInfo);

            // Binding 3: G-Buffer PBR
            VkDescriptorImageInfo pbrImageInfo{
                .sampler = defaultSampler,
                .imageView = gPBRTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(3, &pbrImageInfo);

            // Binding 4: G-Buffer Emissive
            VkDescriptorImageInfo emissiveImageInfo{
                .sampler = defaultSampler,
                .imageView = gEmissiveTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(4, &emissiveImageInfo);

            // Binding 5: G-Buffer Depth
            VkDescriptorImageInfo depthImageInfo{
                .sampler = defaultSampler,
                .imageView = gDepthTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(5, &depthImageInfo);

            // Binding 6: Light SSBO
            const Buffer& lightBuffer = uniform.GetUniformBuffer(6, frameIndex);
            VkDescriptorBufferInfo lightBufferInfo{
                .buffer = lightBuffer.buffer,
                .offset = 0,
                .range = lightBuffer.bufferSize
            };
            writer.WriteBuffer(6, &lightBufferInfo);

            VkDescriptorImageInfo shadowInfo{
                .sampler = defaultSampler,
                .imageView = shadowMomentsTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(7, &shadowInfo);

            // Binding 8: Shadow params UBO
            const Buffer& shadowBuf = uniform.GetUniformBuffer(8, frameIndex);
            VkDescriptorBufferInfo shadowBufInfo{
                .buffer = shadowBuf.buffer,
                .offset = 0,
                .range = shadowBuf.bufferSize
            };
            writer.WriteBuffer(8, &shadowBufInfo);

            VkDescriptorImageInfo envMapInfo{
                .sampler = defaultSampler,
                .imageView = envMapTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(9, &envMapInfo);

            VkDescriptorImageInfo irMapInfo{
                .sampler = defaultSampler,
                .imageView = irMapTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(10, &irMapInfo);

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void DeferredLightingUniformUpdate(Uniform& uniform, RenderingResource& renderData)
    {
        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();

        // Get G-Buffer textures (with new image views after resize)
        VKTexture* gAlbedoTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gAlbedo"));
        VKTexture* gNormalTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gNormal"));
        VKTexture* gPBRTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gPBR"));
        VKTexture* gEmissiveTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("gEmissive"));
        VKTexture* gDepthTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("SceneDepth"));
        VKTexture* shadowMomentsTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("ShadowMoments"));
        VKTexture* shadowDepthTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("ShadowDepth"));
        
        VKTexture* envMapTex = static_cast<VKTexture*>(
            renderData.textureLibrary->GetTextureByIndex(renderData.envMapIndex)
        );

        VKTexture* irMapTex = static_cast<VKTexture*>(
            renderData.textureLibrary->GetTextureByIndex(renderData.irMapIndex)
        );

        if (!gAlbedoTex || !gNormalTex || !gPBRTex || !gEmissiveTex || !gDepthTex || !shadowMomentsTex || !shadowDepthTex || !envMapTex || !irMapTex)
        {
            RADIS_ERROR("One or more textures not found!");
            return;
        }

        // Update descriptor sets for each frame with new image views
        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            // Binding 0: Camera UBO
            const Buffer& cameraBuffer = uniform.GetUniformBuffer(0, frameIndex);
            VkDescriptorBufferInfo cameraBufferInfo{
                .buffer = cameraBuffer.buffer,
                .offset = 0,
                .range = cameraBuffer.bufferSize
            };
            writer.WriteBuffer(0, &cameraBufferInfo);

            // Binding 1: G-Buffer Albedo
            VkDescriptorImageInfo albedoImageInfo{
                .sampler = defaultSampler,
                .imageView = gAlbedoTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(1, &albedoImageInfo);

            // Binding 2: G-Buffer Normal
            VkDescriptorImageInfo normalImageInfo{
                .sampler = defaultSampler,
                .imageView = gNormalTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(2, &normalImageInfo);

            // Binding 3: G-Buffer PBR
            VkDescriptorImageInfo pbrImageInfo{
                .sampler = defaultSampler,
                .imageView = gPBRTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(3, &pbrImageInfo);

            // Binding 4: G-Buffer Emissive
            VkDescriptorImageInfo emissiveImageInfo{
                .sampler = defaultSampler,
                .imageView = gEmissiveTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(4, &emissiveImageInfo);

            // Binding 5: G-Buffer Depth
            VkDescriptorImageInfo depthImageInfo{
                .sampler = defaultSampler,
                .imageView = gDepthTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(5, &depthImageInfo);

            // Binding 6: Light SSBO
            const Buffer& lightBuffer = uniform.GetUniformBuffer(6, frameIndex);
            VkDescriptorBufferInfo lightBufferInfo{
                .buffer = lightBuffer.buffer,
                .offset = 0,
                .range = lightBuffer.bufferSize
            };
            writer.WriteBuffer(6, &lightBufferInfo);

            VkDescriptorImageInfo shadowInfo{
                .sampler = defaultSampler,
                .imageView = shadowMomentsTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(7, &shadowInfo);

            // Binding 8: Shadow params UBO
            const Buffer& shadowBuf = uniform.GetUniformBuffer(8, frameIndex);
            VkDescriptorBufferInfo shadowBufInfo{
                .buffer = shadowBuf.buffer,
                .offset = 0,
                .range = shadowBuf.bufferSize
            };

            VkDescriptorImageInfo envMapInfo{
                .sampler = defaultSampler,
                .imageView = envMapTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(9, &envMapInfo);

            VkDescriptorImageInfo irMapInfo{
                .sampler = defaultSampler,
                .imageView = irMapTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(10, &irMapInfo);

            writer.Overwrite(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void TonemapUniformInit(Uniform& uniform, RenderingResource& renderData)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();
        VKTexture* sceneHDRTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("SceneHDR"));

        if (!sceneHDRTex)
        {
            RADIS_ERROR("TonemapUniformInit: SceneHDR texture not found!");
            return;
        }

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            VkDescriptorImageInfo imageInfo{
                .sampler = defaultSampler,
                .imageView = sceneHDRTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(0, &imageInfo);

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void TonemapUniformUpdate(Uniform& uniform, RenderingResource& renderData)
    {
        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();
        VKTexture* sceneHDRTex = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture("SceneHDR"));

        if (!sceneHDRTex)
        {
            RADIS_ERROR("TonemapUniformUpdate: SceneHDR texture not found!");
            return;
        }

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            VkDescriptorImageInfo imageInfo{
                .sampler = defaultSampler,
                .imageView = sceneHDRTex->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            writer.WriteImage(0, &imageInfo);

            writer.Overwrite(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void ShadowMomentsUniformInit(Uniform& uniform, RenderingResource& rd)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            // Binding 0: shadow UBO
            const Buffer& ubo = uniform.GetUniformBuffer(0, frame);
            VkDescriptorBufferInfo uboInfo{
                .buffer = ubo.buffer,
                .offset = 0,
                .range = ubo.bufferSize
            };
            writer.WriteBuffer(0, &uboInfo);

            // Binding 1: instance SSBO
            const Buffer& inst = uniform.GetUniformBuffer(1, frame);
            VkDescriptorBufferInfo instInfo{
                .buffer = inst.buffer,
                .offset = 0,
                .range = inst.bufferSize
            };
            writer.WriteBuffer(1, &instInfo);

            // Binding 2: bone SSBO
            const Buffer& bones = uniform.GetUniformBuffer(2, frame);
            VkDescriptorBufferInfo boneInfo{
                .buffer = bones.buffer,
                .offset = 0,
                .range = bones.bufferSize
            };
            writer.WriteBuffer(2, &boneInfo);

            writer.Build(uniform.GetDescriptorSets()[frame]);
        }
    }

    void ShadowBlurUniformInitH(Uniform& uniform, RenderingResource& rd)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
        VkSampler sampler = rd.textureLibrary->GetSampler();

        auto* src = static_cast<VKTexture*>(rd.textureLibrary->GetTexture("ShadowMomentsRaw"));
        auto* dst = static_cast<VKTexture*>(rd.textureLibrary->GetTexture("ShadowMomentsTmp"));

        for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            VkDescriptorImageInfo srcInfo{ sampler, src->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkDescriptorImageInfo dstInfo{ VK_NULL_HANDLE, dst->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };

            writer.WriteImage(0, &srcInfo);
            writer.WriteImage(1, &dstInfo);
            writer.Build(uniform.GetDescriptorSets()[frame]);
        }
    }

    void ShadowBlurUniformInitV(Uniform& uniform, RenderingResource& rd)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
        VkSampler sampler = rd.textureLibrary->GetSampler();

        auto* src = static_cast<VKTexture*>(rd.textureLibrary->GetTexture("ShadowMomentsTmp"));
        auto* dst = static_cast<VKTexture*>(rd.textureLibrary->GetTexture("ShadowMoments"));

        for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            VkDescriptorImageInfo srcInfo{ sampler, src->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkDescriptorImageInfo dstInfo{ VK_NULL_HANDLE, dst->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };

            writer.WriteImage(0, &srcInfo);
            writer.WriteImage(1, &dstInfo);
            writer.Build(uniform.GetDescriptorSets()[frame]);
        }
    }


}