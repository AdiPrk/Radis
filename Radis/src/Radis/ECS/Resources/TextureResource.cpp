#include "PCH/pch.h"
#include "TextureResource.h"
#include "RenderingResource.h"
#include "Graphics/Common/TextureLibrary.h"

#include "Graphics/RHI/ITexture.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"
#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"

namespace Radis
{

    TextureResource::TextureResource(RenderingResource& rr)
        : device{ rr.device.get() }
        , textureUniform{ rr.cameraUniform.get() }
    {
        CreateTextureSampler();
        CreateDescriptors();
    }

    TextureResource::~TextureResource()
    {
    }

    void TextureResource::Shutdown()
    {
        if (mTextureSampler && device)
        {
            vkDestroySampler(device->GetDevice(), mTextureSampler, nullptr);
            mTextureSampler = VK_NULL_HANDLE;
        }
        if (mImageDescriptorSetLayout && device)
        {
            vkDestroyDescriptorSetLayout(device->GetDevice(), mImageDescriptorSetLayout, nullptr);
            vkDestroyDescriptorPool(device->GetDevice(), mImageDescriptorPool, nullptr);
            mImageDescriptorSetLayout = VK_NULL_HANDLE;
            mImageDescriptorPool = VK_NULL_HANDLE;
        }

        mTextures.clear();
    }

    void TextureResource::CreateTextureSampler()
    {
        if (mTextureSampler != VK_NULL_HANDLE)
        {
            return;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device->GetPhysicalDevice(), &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = -VK_LOD_CLAMP_NONE;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        if (vkCreateSampler(device->GetDevice(), &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
        {
            RADIS_CRITICAL("Failed to create texture sampler");
        }
    }

    void TextureResource::CreateDescriptors()
    {
        if (mImageDescriptorPool != VK_NULL_HANDLE)
        {
            return;
        }

        // 1. Define the layout binding
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0; // The binding point in the shader (e.g., layout(binding = 0) ...)
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1; // You're binding one sampler
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Accessible in the fragment shader
        samplerLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // 2. Create the descriptor set layout
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(device->GetDevice(), &layoutInfo, nullptr, &mImageDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        // 1. Define the pool size
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = TextureLibrary::MAX_TEXTURE_COUNT; // Enough space for one descriptor of this type

        // 2. Create the descriptor pool info
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = TextureLibrary::MAX_TEXTURE_COUNT; // Max number of descriptor sets that can be allocated

        if (vkCreateDescriptorPool(device->GetDevice(), &poolInfo, nullptr, &mImageDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void TextureResource::CreateDescriptorSet(VKTexture* texture, VkImageLayout layout)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mImageDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mImageDescriptorSetLayout; // Use the layout from step 1

        if (vkAllocateDescriptorSets(device->GetDevice(), &allocInfo, &texture->mDescriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor set!");
        }

        // 1. Populate the image info struct
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = layout; // Layout the image will be in when sampled
        imageInfo.imageView = texture->GetImageView(); // Your VkImageView handle
        imageInfo.sampler = mTextureSampler;     // Your VkSampler handle

        // 2. Populate the write descriptor struct
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = texture->mDescriptorSet; // The set to update (from step 3)
        descriptorWrite.dstBinding = 0;         // The binding to update (from step 1)
        descriptorWrite.dstArrayElement = 0;  // Start at index 0
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo; // Point to the image info struct

        // 3. Call vkUpdateDescriptorSets to perform the update
        vkUpdateDescriptorSets(device->GetDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    void TextureResource::UpdateTextureUniform(uint32_t assetID, TextureData& asset)
    {
        if (assetID >= TextureLibrary::MAX_TEXTURE_COUNT) 
        {
            RADIS_ERROR("AssetID {} exceeds the maximum texture count of {}", assetID, TextureLibrary::MAX_TEXTURE_COUNT);
            return;
        }

        const TextureData& textureData = asset;
        if (textureData.name.empty() && textureData.pixels.size() == 0) return;

        VkImageLayout finalLayout = (textureData.isStorageImage || textureData.isSpecialImage) ? textureData.finalLayout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        mTextures.push_back(std::make_unique<VKTexture>(*device, textureData));
        CreateDescriptorSet(static_cast<VKTexture*>(mTextures.back().get()), finalLayout);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = mTextures.back()->GetImageView();
        imageInfo.sampler = mTextureSampler;

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = textureUniform->GetDescriptorSets()[frameIndex];
            descriptorWrite.dstBinding = 3;

            descriptorWrite.dstArrayElement = assetID;

            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            // Perform the targeted update instantly
            vkUpdateDescriptorSets(device->GetDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }

}