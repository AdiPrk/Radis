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

        ITexture* itex0 = renderData.textureLibrary->GetTextureByIndex(0);
        VKTexture* vktex0 = static_cast<VKTexture*>(itex0);

        std::vector<VkDescriptorImageInfo> imageInfos(TextureLibrary::MAX_TEXTURE_COUNT);
        for (size_t j = 0; j < TextureLibrary::MAX_TEXTURE_COUNT; ++j) 
        {
            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[j].sampler = defaultSampler;

            ITexture* itex = renderData.textureLibrary->GetTextureByIndex(static_cast<uint32_t>(std::min(j, textureCount - 1)));
            VKTexture* vktex = static_cast<VKTexture*>(itex);
            if (vktex)
            {
                imageInfos[j].imageView = vktex->GetImageView();
            }
            else
            {
                imageInfos[j].imageView = vktex0 ? vktex0->GetImageView() : VK_NULL_HANDLE;
            }
        }

        // Build descriptor sets for each frame with both buffer and texture data
        for(int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            writer.WriteBuffer(0, uniform.GetUniformBuffer(0, frameIndex));
            writer.WriteBuffer(1, uniform.GetUniformBuffer(1, frameIndex));
            writer.WriteBuffer(2, uniform.GetUniformBuffer(2, frameIndex));
            writer.WriteImage(3, imageInfos.data(), static_cast<uint32_t>(imageInfos.size()));
            writer.WriteBuffer(4, uniform.GetUniformBuffer(4, frameIndex));

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void RTUniformInit(Uniform& uniform, RenderingResource& renderData)
    {
        VKTexture* sceneHDRTex = renderData.textureLibrary->GetVKTexture("SceneHDR");
        VKTexture* envMapTex = renderData.textureLibrary->GetVKTexture(Assets::ImagesPath + "Newport_Loft_Ref.hdr");
        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();

        if (!sceneHDRTex || !envMapTex) return;

        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            int historyIndex = (frameIndex + SwapChain::MAX_FRAMES_IN_FLIGHT - 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;

            VKTexture* rtAccumCurrent = renderData.textureLibrary->GetVKTexture("RTAccum_" + std::to_string(frameIndex));
            VKTexture* rtAccumHistory = renderData.textureLibrary->GetVKTexture("RTAccum_" + std::to_string(historyIndex));
            VKTexture* rtHeatmapCurrent = renderData.textureLibrary->GetVKTexture("RTHeatmapImage_" + std::to_string(frameIndex));

            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            // binding 0 is handled in rendersystem
            writer.WriteImage(1, rtAccumCurrent, defaultSampler, VK_IMAGE_LAYOUT_GENERAL)
                .WriteImage(2, rtHeatmapCurrent, defaultSampler, VK_IMAGE_LAYOUT_GENERAL)
                .WriteBuffer(3, uniform.GetUniformBuffer(3, frameIndex)) // Vertices SSBO
                .WriteBuffer(4, uniform.GetUniformBuffer(4, frameIndex)) // Indices SSBO
                .WriteImage(5, rtAccumHistory, defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .WriteImage(6, sceneHDRTex, defaultSampler, VK_IMAGE_LAYOUT_GENERAL) // Output to SceneHDR
                .WriteImage(7, envMapTex, defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }
}