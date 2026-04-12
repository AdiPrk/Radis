/*****************************************************************//**
 * \file   TextureLibrary.cpp
 * \brief  Implementation of the TextureLibrary class for managing texture resources.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "TextureLibrary.h"
#include "ECS/Resources/RenderingResource.h"

#include "../RHI/ITexture.h"
#include "../Vulkan/Texture/VKTexture.h"
#include "../Vulkan/Core/Device.h"
#include "../Vulkan/Core/SwapChain.h"
#include "../Vulkan/Uniform/Descriptors.h"
#include "../Vulkan/Uniform/Uniform.h"

#include "TextureLoader.h"
#include "Engine.h"

namespace Radis
{
    const uint32_t TextureLibrary::MAX_TEXTURE_COUNT = 500;
    const uint32_t TextureLibrary::INVALID_TEXTURE_INDEX = 10001;

    TextureLibrary::TextureLibrary(Device* device)
        : device{ device }
        , mTextureSampler{ VK_NULL_HANDLE }
    {
        mTexturesData.resize(MAX_TEXTURE_COUNT);
        mTextures.resize(MAX_TEXTURE_COUNT);

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            CreateTextureSampler();
            CreateDescriptors();
        }
    }

    TextureLibrary::~TextureLibrary()
    {
        if (mTextureSampler && device)
        {
            vkDestroySampler(device->GetDevice(), mTextureSampler, nullptr);
        }
        if (mImageDescriptorSetLayout && device)
        {
            vkDestroyDescriptorSetLayout(device->GetDevice(), mImageDescriptorSetLayout, nullptr);
            vkDestroyDescriptorPool(device->GetDevice(), mImageDescriptorPool, nullptr);
        }

        mTextures.clear();
    }

    uint32_t TextureLibrary::QueueTextureLoad(const std::string& texturePath)
    {
        if (mTextureMap.find(texturePath) != mTextureMap.end())
        {
            return mTextureMap[texturePath];
        }

        uint32_t index = mNextIndex++;
        mTextureMap[texturePath] = index;

        if (mTexturesData.size() <= index)
        {
            mTexturesData.resize(index + 1);
        }

        TextureLoadData& loadData = mPendingTextureLoads.emplace_back();
        loadData.path = texturePath;
        loadData.targetIndex = index;
        return index;
    }

    uint32_t TextureLibrary::QueueTextureLoad(const unsigned char* textureData, uint32_t textureSize, const std::string& texturePath)
    {
        if (mTextureMap.find(texturePath) != mTextureMap.end())
        {
            return mTextureMap[texturePath];
        }

        uint32_t index = mNextIndex++;
        mTextureMap[texturePath] = index;

        if (mTexturesData.size() <= index)
        {
            mTexturesData.resize(index + 1);
        }
        if (mTexturesData[index].isSpecialImage)
        {
            return index;
        }

        TextureLoadData& loadData = mPendingTextureLoads.emplace_back();
        loadData.data = textureData;
        loadData.size = textureSize;
        loadData.path = texturePath;
        loadData.targetIndex = index;
        return index;
    }

    bool TextureLibrary::LoadQueuedTextures()
    {
        if (mPendingTextureLoads.empty())
        {
            return false;
        }

        RADIS_INFO("Loading {0} queued textures...", mPendingTextureLoads.size());
        TextureLoader::LoadMT(mPendingTextureLoads);

        RADIS_INFO("Finished loading textures. Creating GPU resources...");
        mNeedTextureDescriptorUpdate = true;
        if (mTextures.size() < mNextIndex)
        {
            mTextures.resize(mNextIndex);
        }

        for (auto& loadData : mPendingTextureLoads)
        {
            uint32_t index = loadData.targetIndex;

            mTexturesData[index] = std::move(loadData.outTexture);
            mTexturesData[index].name = loadData.path;

            std::unique_ptr<ITexture> newTexture;
            newTexture = std::make_unique<VKTexture>(*device, mTexturesData[index]);
            CreateDescriptorSet(static_cast<VKTexture*>(newTexture.get()));

            mTextures[index] = std::move(newTexture);
        }

        RADIS_INFO("All queued textures loaded successfully!");
        mPendingTextureLoads.clear();
        return true;
    }

    uint32_t TextureLibrary::CreateStorageImage(const std::string& imageName, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageUsageFlags usage, VkImageLayout finalLayout)
    {
        // check if exists
        auto it = mTextureMap.find(imageName);
        if (it != mTextureMap.end())
        {
            return it->second;
        }

        uint32_t index = mNextIndex++;
        mTextureMap[imageName] = index;
        if (mTexturesData.size() <= index)
        {
            mTexturesData.resize(index + 1);
        }

        TextureData& textureData = mTexturesData[index];
        textureData.isStorageImage = true;
        textureData.width = width;
        textureData.height = height;
        textureData.imageFormat = imageFormat;
        textureData.usage = usage;
        textureData.finalLayout = finalLayout;
        textureData.name = imageName;

        if (mTextures.size() <= index)
            mTextures.resize(index + 1);

        mTextures[index] = std::make_unique<VKTexture>(*device, textureData);
        CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), finalLayout);

        return index;
    }

    void TextureLibrary::ResizeStorageImage(const std::string& imageName, uint32_t newWidth, uint32_t newHeight)
    {
        auto it = mTextureMap.find(imageName);
        if (it == mTextureMap.end())
        {
            return;
        }

        mNeedTextureDescriptorUpdate = true;

        uint32_t index = it->second;

        TextureData& tex = mTexturesData[index];
        tex.width = newWidth;
        tex.height = newHeight;

        // vkFreeDescriptorSets(device->GetDevice(), mImageDescriptorPool, 1, &static_cast<VKTexture*>(mTextures[index].get())->mDescriptorSet);
        mTextures[index].reset();

        mTextures[index] = std::make_unique<VKTexture>(*device, tex);
        CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), tex.finalLayout);
    }

    uint32_t TextureLibrary::CreateTexture(const std::string& imageName, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout finalLayout)
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            RADIS_ERROR("CreateTexture called for non-Vulkan API");
            return INVALID_TEXTURE_INDEX;
        }

        auto it = mTextureMap.find(imageName);
        if (it != mTextureMap.end())
        {
            return it->second;
        }

        uint32_t index = mNextIndex++;
        mTextureMap[imageName] = index;
        if (mTexturesData.size() <= index)
        {
            mTexturesData.resize(index + 1);
        }

        TextureData& textureData = mTexturesData[index];
        textureData.isSpecialImage = true; // Use this to flag as procedural
        textureData.width = width;
        textureData.height = height;
        textureData.imageFormat = imageFormat;
        textureData.tiling = tiling;
        textureData.usage = usage;
        textureData.finalLayout = finalLayout;
        textureData.name = imageName;

        if (mTextures.size() <= index)
            mTextures.resize(index + 1);

        mTextures[index] = std::make_unique<VKTexture>(*device, textureData);
        CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), finalLayout);

        return index;
    }

    void TextureLibrary::ResizeTexture(const std::string& imageName, uint32_t newWidth, uint32_t newHeight)
    {
        auto it = mTextureMap.find(imageName);
        if (it == mTextureMap.end())
        {
            return;
        }

        mNeedReuploadRTImage = true; // This seems related to RT, you may want to check this logic

        uint32_t index = it->second;

        TextureData& tex = mTexturesData[index];
        tex.width = newWidth;
        tex.height = newHeight;

        // Check if the old texture had a descriptor set before we destroy it
        VKTexture* oldTexture = static_cast<VKTexture*>(mTextures[index].get());
        bool hadShaderDescriptor = (oldTexture && oldTexture->mDescriptorSet != VK_NULL_HANDLE);

        // vkFreeDescriptorSets... (your commented-out line)
        mTextures[index].reset();

        mTextures[index] = std::make_unique<VKTexture>(*device, tex);

        // If it had a descriptor set before, recreate one
        if (hadShaderDescriptor)
        {
            CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), tex.finalLayout);
        }
    }

    ITexture* TextureLibrary::GetTexture(uint32_t index)
    {
        if (index < mTextures.size()) {
            return mTextures[index].get();
        }
        return nullptr;
    }

    ITexture* TextureLibrary::GetTexture(const std::string& filePath)
    {
        auto it = mTextureMap.find(filePath);
        if (it != mTextureMap.end()) {
            return GetTexture(it->second);
        }
        return nullptr;
    }

    ITexture* TextureLibrary::GetTextureByIndex(uint32_t index)
    {
        if (index < mTextures.size()) {
            return mTextures[index].get();
        }

        RADIS_ERROR("Texture index out of range: {0}", index);
        return mTextures[0].get();
    }

    void TextureLibrary::CreateTextureSampler()
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            //RADIS_WARN("CreateTextureSampler called for non-Vulkan API");
            return;
        }

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

    void TextureLibrary::CreateDescriptors()
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            //RADIS_WARN("CreateTextureSampler called for non-Vulkan API");
            return;
        }

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
        poolSize.descriptorCount = MAX_TEXTURE_COUNT; // Enough space for one descriptor of this type

        // 2. Create the descriptor pool info
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = MAX_TEXTURE_COUNT; // Max number of descriptor sets that can be allocated

        if (vkCreateDescriptorPool(device->GetDevice(), &poolInfo, nullptr, &mImageDescriptorPool) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void TextureLibrary::CreateDescriptorSet(VKTexture* texture, VkImageLayout layout)
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

    void TextureLibrary::UpdateTextureUniform(Uniform* uniform)
    {
        if (!mNeedTextureDescriptorUpdate) return;
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan) return;

        size_t textureCount = GetTextureCount();
        VkSampler defaultSampler = GetSampler();
        bool hasTex = textureCount > 0;
        mNeedTextureDescriptorUpdate = false;

        std::vector<VkDescriptorImageInfo> imageInfos(TextureLibrary::MAX_TEXTURE_COUNT);
        for (size_t j = 0; j < TextureLibrary::MAX_TEXTURE_COUNT; ++j) {
            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[j].sampler = defaultSampler;
            imageInfos[j].imageView = 0;

            if (hasTex)
            {
                ITexture* itex = GetTextureByIndex(static_cast<uint32_t>(std::min(j, textureCount - 1)));
                VKTexture* vktex = static_cast<VKTexture*>(itex);
                if (vktex)
                {
                    if (vktex->mData.name == "SceneTexture") continue;
                    if (vktex->mData.name == "SceneDepth") continue;
                    if (vktex->mData.name == "RTAccum_0") continue;
                    if (vktex->mData.name == "RTAccum_1") continue;
                    if (vktex->mData.name == "RTHeatmapImage_0") continue;
                    if (vktex->mData.name == "RTHeatmapImage_1") continue;

                    imageInfos[j].imageView = vktex->GetImageView();
                }
            }
        }

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex) {
            DescriptorWriter writer(*uniform->GetDescriptorLayout(), *uniform->GetDescriptorPool());
            writer.WriteImage(3, imageInfos.data(), static_cast<uint32_t>(imageInfos.size()));
            writer.Overwrite(uniform->GetDescriptorSets()[frameIndex]);
        }
    }

    void TextureLibrary::ClearAllBuffers(class Device* device)
    {
        if (mTextureSampler && device)
        {
            vkDestroySampler(device->GetDevice(), mTextureSampler, nullptr);
            mTextureSampler = VK_NULL_HANDLE;
        }

        if (mImageDescriptorSetLayout && device)
        {
            vkDestroyDescriptorSetLayout(device->GetDevice(), mImageDescriptorSetLayout, nullptr);
            mImageDescriptorSetLayout = VK_NULL_HANDLE;
        }

        if (mImageDescriptorPool && device)
        {
            vkDestroyDescriptorPool(device->GetDevice(), mImageDescriptorPool, nullptr);
            mImageDescriptorPool = VK_NULL_HANDLE;
        }

        mTextures.clear();
    }

    void TextureLibrary::RecreateAllBuffers(class Device* device)
    {
        mTextures.resize(mTexturesData.size());

        for (uint32_t index = 0; index < mTexturesData.size(); ++index)
        {
            TextureData& textureData = mTexturesData[index];
            if (textureData.name.empty() && textureData.pixels.size() == 0) continue;

            VkImageLayout finalLayout = (textureData.isStorageImage || textureData.isSpecialImage) ? textureData.finalLayout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            mTextures[index] = std::make_unique<VKTexture>(*device, textureData);
            CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), finalLayout);

            // Ensure mTextureMap has the same mapping
            mTextureMap[textureData.name] = index;
        }
    }

    void TextureLibrary::UpdateRTUniform(RenderingResource& rr)
    {
        if (!mNeedReuploadRTImage) return;
        mNeedReuploadRTImage = false;

        std::vector<VKTexture*> rtAccumTextures(SwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VKTexture*> rtHeatmapTextures(SwapChain::MAX_FRAMES_IN_FLIGHT);

        // 1. Fetch the renamed Ping-Pong accumulation textures
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            std::string texName = "RTAccum_" + std::to_string(i);
            rtAccumTextures[i] = static_cast<VKTexture*>(rr.textureLibrary->GetTexture(texName));
        }

        // 2. Fetch the Heatmap textures
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            std::string texName = "RTHeatmapImage_" + std::to_string(i);
            rtHeatmapTextures[i] = static_cast<VKTexture*>(rr.textureLibrary->GetTexture(texName));
        }

        // 3. Fetch the SceneHDR (Tonemapper bridge) and the sampler
        VKTexture* sceneHDRTex = static_cast<VKTexture*>(rr.textureLibrary->GetTexture("SceneHDR"));
        VkSampler defaultSampler = rr.textureLibrary->GetSampler();

        VKTexture* envMapTex = static_cast<VKTexture*>(
            rr.textureLibrary->GetTextureByIndex(rr.envMapIndex)
          );

        if (!envMapTex)
        {
            RADIS_ERROR("Environment map texture not found!");
            return;
        }

        for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            int historyIndex = (frameIndex + SwapChain::MAX_FRAMES_IN_FLIGHT - 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;

            // Binding 1: SceneHDR (Write)
            VkDescriptorImageInfo outSceneHDRInfo{};
            outSceneHDRInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outSceneHDRInfo.sampler = VK_NULL_HANDLE;
            outSceneHDRInfo.imageView = sceneHDRTex->GetImageView();

            // Binding 2: Heatmap (Write)
            VkDescriptorImageInfo heatmapImageInfo{};
            heatmapImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            heatmapImageInfo.sampler = VK_NULL_HANDLE;
            heatmapImageInfo.imageView = rtHeatmapTextures[frameIndex]->GetImageView();

            // Binding 5: History Read (Sampler)
            VkDescriptorImageInfo historyReadInfo{};
            historyReadInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            historyReadInfo.sampler = defaultSampler;
            historyReadInfo.imageView = rtAccumTextures[historyIndex]->GetImageView();

            // Binding 6: History Write (Storage)
            VkDescriptorImageInfo historyWriteInfo{};
            historyWriteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            historyWriteInfo.sampler = VK_NULL_HANDLE;
            historyWriteInfo.imageView = rtAccumTextures[frameIndex]->GetImageView();

            VkDescriptorImageInfo envMapInfo{};
            envMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            envMapInfo.sampler = defaultSampler;
            envMapInfo.imageView = envMapTex->GetImageView();

            DescriptorWriter writer(*rr.rtUniform->GetDescriptorLayout(), *rr.rtUniform->GetDescriptorPool());

            // Write the updated image descriptors to their respective bindings
            writer.WriteImage(1, &outSceneHDRInfo);
            writer.WriteImage(2, &heatmapImageInfo);
            writer.WriteImage(5, &historyReadInfo);
            writer.WriteImage(6, &historyWriteInfo);
            writer.WriteImage(7, &envMapInfo);

            writer.Overwrite(rr.rtUniform->GetDescriptorSets()[frameIndex]);
        }
    }
}
