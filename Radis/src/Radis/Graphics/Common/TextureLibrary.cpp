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
    const uint32_t TextureLibrary::INVALID_TEXTURE_INDEX = UINT32_MAX;

    TextureLibrary::TextureLibrary(Device* device)
        : device{ device }
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
        if (device)
        {
            if (mTextureSampler != VK_NULL_HANDLE)
                vkDestroySampler(device->GetDevice(), mTextureSampler, nullptr);

            if (mImageDescriptorSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(device->GetDevice(), mImageDescriptorSetLayout, nullptr);

            if (mImageDescriptorPool != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(device->GetDevice(), mImageDescriptorPool, nullptr);
        }

        mTextures.clear();
    }

    uint32_t TextureLibrary::AllocateSlot(const std::string& name)
    {
        auto [it, inserted] = mTextureMap.emplace(name, mNextIndex);
        if (!inserted)
            return it->second;

        RADIS_ASSERT(mNextIndex < MAX_TEXTURE_COUNT, "TextureLibrary: MAX_TEXTURE_COUNT ({0}) exceeded", MAX_TEXTURE_COUNT);

        return mNextIndex++;
    }

    void TextureLibrary::InstantiateTexture(uint32_t index, VkImageLayout layout)
    {
        mTextures[index] = std::make_unique<VKTexture>(*device, mTexturesData[index]);
        CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), layout);
    }

    void TextureLibrary::RecreateAtIndex(uint32_t index, uint32_t newWidth, uint32_t newHeight)
    {
        TextureData& td = mTexturesData[index];
        td.width = newWidth;
        td.height = newHeight;

        VKTexture* old = static_cast<VKTexture*>(mTextures[index].get());
        bool       hadDescriptor = old && (old->mDescriptorSet != VK_NULL_HANDLE);

        mTextures[index].reset();
        mTextures[index] = std::make_unique<VKTexture>(*device, td);

        if (hadDescriptor)
            CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), td.finalLayout);
    }

    uint32_t TextureLibrary::QueueTextureLoad(const std::string& texturePath)
    {
        auto [it, inserted] = mTextureMap.emplace(texturePath, mNextIndex);
        if (!inserted)
            return it->second;

        uint32_t index = mNextIndex++;

        TextureLoadData& load = mPendingTextureLoads.emplace_back();
        load.path = texturePath;
        load.targetIndex = index;
        return index;
    }

    uint32_t TextureLibrary::QueueTextureLoad(const unsigned char* data, uint32_t size,
        const std::string& texturePath)
    {
        auto [it, inserted] = mTextureMap.emplace(texturePath, mNextIndex);
        if (!inserted)
            return it->second;

        uint32_t index = mNextIndex++;

        TextureLoadData& load = mPendingTextureLoads.emplace_back();
        load.data = data;
        load.size = size;
        load.path = texturePath;
        load.targetIndex = index;
        return index;
    }

    bool TextureLibrary::LoadQueuedTextures()
    {
        if (mPendingTextureLoads.empty())
            return false;

        RADIS_INFO("Loading {0} queued textures...", mPendingTextureLoads.size());
        TextureLoader::LoadMT(mPendingTextureLoads);
        RADIS_INFO("Finished CPU decode. Uploading to GPU...");

        mNeedTextureDescriptorUpdate = true;

        for (auto& load : mPendingTextureLoads)
        {
            const uint32_t index = load.targetIndex;

            mTexturesData[index] = std::move(load.outTexture);
            mTexturesData[index].name = load.path;

            InstantiateTexture(index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        RADIS_INFO("All queued textures loaded successfully!");
        mPendingTextureLoads.clear();
        return true;
    }

    uint32_t TextureLibrary::CreateStorageImage(const std::string& name,
        uint32_t width, uint32_t height,
        VkFormat format, VkImageUsageFlags usage,
        VkImageLayout finalLayout)
    {
        auto [it, inserted] = mTextureMap.emplace(name, mNextIndex);
        if (!inserted)
            return it->second;

        const uint32_t index = mNextIndex++;

        TextureData& td = mTexturesData[index];
        td.name = name;
        td.isStorageImage = true;
        td.width = width;
        td.height = height;
        td.imageFormat = format;
        td.usage = usage;
        td.finalLayout = finalLayout;

        InstantiateTexture(index, finalLayout);
        return index;
    }

    uint32_t TextureLibrary::CreateTexture(const std::string& name,
        uint32_t width, uint32_t height,
        VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkImageLayout finalLayout)
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            RADIS_ERROR("CreateTexture: only supported on Vulkan");
            return INVALID_TEXTURE_INDEX;
        }

        auto [it, inserted] = mTextureMap.emplace(name, mNextIndex);
        if (!inserted)
            return it->second;

        const uint32_t index = mNextIndex++;

        TextureData& td = mTexturesData[index];
        td.name = name;
        td.isSpecialImage = true;
        td.width = width;
        td.height = height;
        td.imageFormat = format;
        td.tiling = tiling;
        td.usage = usage;
        td.finalLayout = finalLayout;

        InstantiateTexture(index, finalLayout);
        return index;
    }

    void TextureLibrary::ResizeStorageImage(const std::string& name,
        uint32_t newWidth, uint32_t newHeight)
    {
        auto it = mTextureMap.find(name);
        if (it == mTextureMap.end()) return;

        mNeedTextureDescriptorUpdate = true;
        RecreateAtIndex(it->second, newWidth, newHeight);
    }

    void TextureLibrary::ResizeTexture(const std::string& name,
        uint32_t newWidth, uint32_t newHeight)
    {
        auto it = mTextureMap.find(name);
        if (it == mTextureMap.end()) return;

        mNeedReuploadRTImage = true;
        RecreateAtIndex(it->second, newWidth, newHeight);
    }

    // =========================================================================
    //  Accessors
    // =========================================================================

    ITexture* TextureLibrary::GetTexture(uint32_t index) const
    {
        if (index < mTextures.size())
            return mTextures[index].get();
        return nullptr;
    }

    ITexture* TextureLibrary::GetTexture(const std::string& path) const
    {
        auto it = mTextureMap.find(path);
        return (it != mTextureMap.end()) ? GetTexture(it->second) : nullptr;
    }

    VKTexture* TextureLibrary::GetVKTexture(const std::string& path) const
    {
        return static_cast<VKTexture*>(GetTexture(path));
    }

    ITexture* TextureLibrary::GetTextureByIndex(uint32_t index) const
    {
        if (index < mNextIndex && index < mTextures.size())
            return mTextures[index].get();

        RADIS_ERROR("GetTextureByIndex: index {0} out of range (count={1})", index, mNextIndex);
        return nullptr;
    }

    void TextureLibrary::UpdateTextureUniform(Uniform* uniform)
    {
        if (!mNeedTextureDescriptorUpdate)                   return;
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan) return;

        mNeedTextureDescriptorUpdate = false;

        const uint32_t textureCount = GetTextureCount();
        VkSampler      defaultSampler = GetSampler();

        std::vector<VkDescriptorImageInfo> imageInfos(MAX_TEXTURE_COUNT);
        for (uint32_t j = 0; j < MAX_TEXTURE_COUNT; ++j)
        {
            const uint32_t clampedIdx = (textureCount > 0)
                ? std::min(j, textureCount - 1)
                : 0;

            VKTexture* vktex = static_cast<VKTexture*>(GetTextureByIndex(clampedIdx));

            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[j].sampler = defaultSampler;
            imageInfos[j].imageView = (vktex && !vktex->mData.isStorageImage)
                ? vktex->GetImageView()
                : VK_NULL_HANDLE;
        }

        for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            DescriptorWriter writer(*uniform->GetDescriptorLayout(),
                *uniform->GetDescriptorPool());
            writer.WriteImage(3, imageInfos.data(), static_cast<uint32_t>(imageInfos.size()));
            writer.Overwrite(uniform->GetDescriptorSets()[frame]);
        }
    }

    void TextureLibrary::UpdateRTUniform(RenderingResource& rr)
    {
        if (!mNeedReuploadRTImage) return;
        mNeedReuploadRTImage = false;

        const int frameCount = SwapChain::MAX_FRAMES_IN_FLIGHT;

        std::vector<VKTexture*> rtAccumTextures(frameCount);
        std::vector<VKTexture*> rtHeatmapTextures(frameCount);
        for (int i = 0; i < frameCount; ++i)
        {
            rtAccumTextures[i] = rr.textureLibrary->GetVKTexture("RTAccum_" + std::to_string(i));
            rtHeatmapTextures[i] = rr.textureLibrary->GetVKTexture("RTHeatmapImage_" + std::to_string(i));
        }

        VKTexture* sceneHDRTex = rr.textureLibrary->GetVKTexture("SceneHDR");
        VKTexture* envMapTex = static_cast<VKTexture*>(
            rr.textureLibrary->GetTextureByIndex(rr.envMapIndex));
        VkSampler  sampler = rr.textureLibrary->GetSampler();

        if (!envMapTex)
        {
            RADIS_ERROR("UpdateRTUniform: environment map not found at index {0}", rr.envMapIndex);
            return;
        }

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const int histIdx = (frame + frameCount - 1) % frameCount;

            auto outSceneHDR = VkDescriptorImageInfo{ VK_NULL_HANDLE, sceneHDRTex->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };
            auto heatmap = VkDescriptorImageInfo{ VK_NULL_HANDLE, rtHeatmapTextures[frame]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };
            auto historyRead = VkDescriptorImageInfo{ sampler, rtAccumTextures[histIdx]->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            auto historyWrite = VkDescriptorImageInfo{ VK_NULL_HANDLE, rtAccumTextures[frame]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL };
            auto envMap = VkDescriptorImageInfo{ sampler, envMapTex->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

            DescriptorWriter writer(*rr.rtUniform->GetDescriptorLayout(), *rr.rtUniform->GetDescriptorPool());
            writer.WriteImage(1, &outSceneHDR);
            writer.WriteImage(2, &heatmap);
            writer.WriteImage(5, &historyRead);
            writer.WriteImage(6, &historyWrite);
            writer.WriteImage(7, &envMap);
            writer.Overwrite(rr.rtUniform->GetDescriptorSets()[frame]);
        }
    }

    // =========================================================================
    //  Swapchain / device recreation
    // =========================================================================

    void TextureLibrary::ClearAllBuffers(class Device* device)
    {
        if (!device) return;

        if (mTextureSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device->GetDevice(), mTextureSampler, nullptr);
            mTextureSampler = VK_NULL_HANDLE;
        }
        if (mImageDescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device->GetDevice(), mImageDescriptorSetLayout, nullptr);
            mImageDescriptorSetLayout = VK_NULL_HANDLE;
        }
        if (mImageDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device->GetDevice(), mImageDescriptorPool, nullptr);
            mImageDescriptorPool = VK_NULL_HANDLE;
        }

        // Destroy GPU objects but keep CPU-side metadata so RecreateAllBuffers
        // can rebuild them from mTexturesData without any re-queuing.
        for (auto& tex : mTextures)
            tex.reset();
    }

    void TextureLibrary::CreateTextureSampler()
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan) return;
        if (mTextureSampler != VK_NULL_HANDLE)               return;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device->GetPhysicalDevice(), &props);

        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.anisotropyEnable = VK_TRUE;
        info.maxAnisotropy = props.limits.maxSamplerAnisotropy;
        info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.minLod = -VK_LOD_CLAMP_NONE;
        info.maxLod = VK_LOD_CLAMP_NONE;

        if (vkCreateSampler(device->GetDevice(), &info, nullptr, &mTextureSampler) != VK_SUCCESS)
            RADIS_CRITICAL("TextureLibrary: failed to create texture sampler");
    }

    void TextureLibrary::CreateDescriptors()
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan) return;
        if (mImageDescriptorPool != VK_NULL_HANDLE)          return;

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(device->GetDevice(), &layoutInfo,
            nullptr, &mImageDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("TextureLibrary: failed to create descriptor set layout");
        }

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = MAX_TEXTURE_COUNT;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = MAX_TEXTURE_COUNT;

        if (vkCreateDescriptorPool(device->GetDevice(), &poolInfo,
            nullptr, &mImageDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("TextureLibrary: failed to create descriptor pool");
        }
    }

    void TextureLibrary::CreateDescriptorSet(VKTexture* texture, VkImageLayout layout)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mImageDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &mImageDescriptorSetLayout;

        if (vkAllocateDescriptorSets(device->GetDevice(), &allocInfo,
            &texture->mDescriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("TextureLibrary: failed to allocate descriptor set");
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = layout;
        imageInfo.imageView = texture->GetImageView();
        imageInfo.sampler = mTextureSampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = texture->mDescriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device->GetDevice(), 1, &write, 0, nullptr);
    }

} // namespace Radis