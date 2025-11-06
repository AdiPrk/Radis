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

            writer.WriteBuffer(0, &bufferInfo0);
            writer.WriteBuffer(1, &bufferInfo1);
            writer.WriteBuffer(2, &bufferInfo2);

            // Texture descriptors unchanged
            writer.WriteImage(3, imageInfos.data(), static_cast<uint32_t>(imageInfos.size()));

            writer.Build(uniform.GetDescriptorSets()[frameIndex]);
        }
    }

    void RTUniformInit(Uniform& uniform, RenderingResource& renderData)
    {
        uniform.GetDescriptorSets().resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        VkDescriptorImageInfo outImageInfo{};
        outImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // storage image layout
        outImageInfo.imageView = VK_NULL_HANDLE;// renderData.rayTracingOutput->GetImageView(); // VKImageView
        outImageInfo.sampler = VK_NULL_HANDLE; // storage images donÅft use samplers

        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &renderData.tlasAccel.accel;

        // Build descriptor sets for each frame with both buffer and texture data
        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            DescriptorWriter writer(*uniform.GetDescriptorLayout(), *uniform.GetDescriptorPool());

            writer.WriteImage(0, &outImageInfo, 1);
            // writer.WriteAccelerationStructure(1, &asInfo);

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