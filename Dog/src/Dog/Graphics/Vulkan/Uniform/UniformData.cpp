#include <PCH/pch.h>

#include "UniformData.h"
#include "Uniform.h"
#include "Descriptors.h"

#include "ECS/Resources/RenderingResource.h"
#include "../Core/Buffer.h"
#include "../Core/SwapChain.h"

#include "../Texture/VKTexture.h"

namespace Dog
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
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;

        // Create a vector to hold the pointers to our new textures
        std::vector<VKTexture*> rtTextures(SwapChain::MAX_FRAMES_IN_FLIGHT);

        // Loop and create one texture for each frame
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            // Give each texture a unique name
            std::string texName = "RayTracingOutput_" + std::to_string(i);
            uint32_t rtInd = renderData.textureLibrary->CreateImage(texName, width, height, format, usage, layout);
            rtTextures[i] = static_cast<VKTexture*>(renderData.textureLibrary->GetTexture(rtInd));
        }

        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        // This info struct will be updated inside the loop
        VkDescriptorImageInfo outImageInfo{};
        outImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // storage image layout
        outImageInfo.sampler = VK_NULL_HANDLE; // storage images donÅft use samplers

        // Build descriptor sets for each frame
        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            outImageInfo.imageView = rtTextures[frameIndex]->GetImageView();

            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());
            writer.WriteImage(0, &outImageInfo, 1);
            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    /*
    void InstanceUniformInit(Uniform& uniform, RenderingResource& renderData)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);


        VkSampler defaultSampler = renderData.textureLibrary->GetSampler();
        size_t textureCount = renderData.textureLibrary->GetTextureCount();

        if (textureCount == 0)
        {
            DOG_ERROR("Should have loaded square.png by here!!!");
        }

        std::vector<VkDescriptorImageInfo> imageInfos(TextureLibrary::MAX_TEXTURE_COUNT);
        for (size_t j = 0; j < TextureLibrary::MAX_TEXTURE_COUNT; ++j) {
            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[j].sampler = defaultSampler;
            imageInfos[j].imageView = renderData.textureLibrary->GetTextureByIndex(static_cast<uint32_t>(std::min(j, textureCount - 1))).GetImageView();
        }

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex) 
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            VkDescriptorBufferInfo bufferInfo = uniform.GetUniformBuffer(1, frameIndex)->DescriptorInfo();
            VkDescriptorBufferInfo bufferInfo2 = uniform.GetUniformBuffer(2, frameIndex)->DescriptorInfo();

            writer.WriteImage(0, imageInfos.data(), static_cast<uint32_t>(imageInfos.size()));
            writer.WriteBuffer(1, &bufferInfo);
            writer.WriteBuffer(2, &bufferInfo2);

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }
    */
}