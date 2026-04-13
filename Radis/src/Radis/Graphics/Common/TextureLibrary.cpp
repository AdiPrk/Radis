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
    const uint32_t TextureLibrary::MAX_TEXTURE_COUNT = 1000;
    const uint32_t TextureLibrary::INVALID_TEXTURE_INDEX = UINT32_MAX;

    // =========================================================================
    //  Construction / Destruction
    // =========================================================================

    TextureLibrary::TextureLibrary(Device* device, ThreadPool& threadPool)
        : device{ device }
        , mThreadPool{ &threadPool }
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
        // Ensure all background decode jobs have finished before we destroy
        // Vulkan objects. Workers may hold a TextureLoadData referencing our memory.
        FlushAll();

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

    // =========================================================================
    //  Internal helpers
    // =========================================================================

    uint32_t TextureLibrary::AllocateSlot(const std::string& name)
    {
        auto [it, inserted] = mTextureMap.emplace(name, mNextIndex);
        if (!inserted)
            return it->second; // already known — return existing index

        RADIS_ASSERT(mNextIndex < MAX_TEXTURE_COUNT,
            "TextureLibrary: MAX_TEXTURE_COUNT ({0}) exceeded", MAX_TEXTURE_COUNT);

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

        if (hadDescriptor) CreateDescriptorSet(static_cast<VKTexture*>(mTextures[index].get()), td.finalLayout);
    }

    void TextureLibrary::UploadBatch(std::vector<DecodedTexture>&& batch)
    {
        if (batch.empty()) return;

        for (auto& decoded : batch)
        {
            const uint32_t index = decoded.targetIndex;
            mTexturesData[index] = std::move(decoded.data);
            mTexturesData[index].name = std::move(decoded.path);
            InstantiateTexture(index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        mNeedTextureDescriptorUpdate = true;
        RADIS_INFO("TextureLibrary: uploaded {0} texture(s) to GPU.", batch.size());
    }

    // =========================================================================
    //  Loading
    // =========================================================================

    uint32_t TextureLibrary::QueueTextureLoad(const std::string& texturePath, LoadPriority priority)
    {
        // AllocateSlot is NOT thread-safe, so this must be called from the main thread.
        // Worker threads only push into mReadyForGPU after decoding is complete.
        auto [it, inserted] = mTextureMap.emplace(texturePath, mNextIndex);
        if (!inserted)
            return it->second;

        const uint32_t index = mNextIndex++;

        if (priority == LoadPriority::Immediate)
        {
            TextureLoadData load;
            load.path = texturePath;
            load.targetIndex = index;

            TextureLoader::Load(load); // blocking decode on the calling thread

            mTexturesData[index] = std::move(load.outTexture);
            mTexturesData[index].name = texturePath;
            InstantiateTexture(index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            mNeedTextureDescriptorUpdate = true;
            return index;
        }

        // Async: dispatch decode to the thread pool. The lambda captures everything
        // by value it needs. When done, it pushes a DecodedTexture into mReadyForGPU
        // under the mutex so the main thread can upload it on a future frame.
        mThreadPool->Submit([this, texturePath, index]()
        {
            TextureLoadData load;
            load.path = texturePath;
            load.targetIndex = index;

            TextureLoader::Load(load); // runs on a worker thread

            std::lock_guard lock(mReadyMutex);
            mReadyForGPU.push_back({ index, std::move(load.outTexture), texturePath });
        });

        return index;
    }

    uint32_t TextureLibrary::QueueTextureLoad(const unsigned char* data, uint32_t size, const std::string& textureName, LoadPriority priority)
    {
        auto [it, inserted] = mTextureMap.emplace(textureName, mNextIndex);
        if (!inserted)
            return it->second;

        const uint32_t index = mNextIndex++;

        if (priority == LoadPriority::Immediate)
        {
            TextureLoadData load;
            load.data = data;
            load.size = size;
            load.path = textureName;
            load.targetIndex = index;

            TextureLoader::Load(load);

            mTexturesData[index] = std::move(load.outTexture);
            mTexturesData[index].name = textureName;
            InstantiateTexture(index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            mNeedTextureDescriptorUpdate = true;
            return index;
        }

        // Copy the bytes into a vector so the caller can free their buffer immediately.
        // The vector is moved into the lambda and lives until the worker is done.
        std::vector<unsigned char> dataCopy(data, data + size);

        mThreadPool->Submit([this, textureName, index, dataCopy = std::move(dataCopy)]() mutable
        {
            TextureLoadData load;
            load.data = dataCopy.data();
            load.size = static_cast<uint32_t>(dataCopy.size());
            load.path = textureName;
            load.targetIndex = index;

            TextureLoader::Load(load);

            std::lock_guard lock(mReadyMutex);
            mReadyForGPU.push_back({ index, std::move(load.outTexture), textureName });
        });

        return index;
    }

    bool TextureLibrary::LoadQueuedTextures()
    {
        // Swap under the lock to minimize contention; workers can immediately push
        // new items while we upload the previous batch on the main thread.
        std::vector<DecodedTexture> ready;
        {
            std::lock_guard lock(mReadyMutex);
            if (mReadyForGPU.empty()) return false;
            ready = std::move(mReadyForGPU);
        }

        UploadBatch(std::move(ready));
        mNeedReuploadRTImage = true;  // Textures were loaded, RT descriptors need updating
        return true;
    }

    void TextureLibrary::FlushAll()
    {
        // Wait for every in-flight decode to complete so mReadyForGPU is fully
        // populated, then drain and upload in one shot.
        mThreadPool->WaitAll();
        LoadQueuedTextures();
    }

    // =========================================================================
    //  Immediate GPU texture creation
    // =========================================================================

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

    // =========================================================================
    //  Resize
    // =========================================================================

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

    // =========================================================================
    //  Descriptor / uniform updates
    // =========================================================================

    void TextureLibrary::UpdateTextureUniform(Uniform* uniform)
    {
        if (!mNeedTextureDescriptorUpdate)                   return;
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan) return;

        mNeedTextureDescriptorUpdate = false;

        const uint32_t textureCount = GetTextureCount();
        VkSampler      defaultSampler = GetSampler();

        std::vector<VkDescriptorImageInfo> imageInfos(MAX_TEXTURE_COUNT);

        ITexture* itex0 = GetTextureByIndex(0);
        VKTexture* vktex0 = static_cast<VKTexture*>(itex0);

        for (uint32_t j = 0; j < MAX_TEXTURE_COUNT; ++j)
        {
            const uint32_t clampedIdx = (textureCount > 0) ? std::min(j, textureCount - 1) : 0;
            VKTexture* vktex = static_cast<VKTexture*>(GetTextureByIndex(clampedIdx));

            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[j].sampler = defaultSampler;
            imageInfos[j].imageView = (vktex && !vktex->mData.isStorageImage)
                ? vktex->GetImageView()
                : (vktex0 ? vktex0->GetImageView() : VK_NULL_HANDLE);
        }

        for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            DescriptorWriter writer(*uniform->GetDescriptorLayout(), *uniform->GetDescriptorPool());
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
        VKTexture* envMapTex = rr.textureLibrary->GetVKTexture(Assets::ImagesPath + "Newport_Loft_Ref.hdr");
        VkSampler  sampler = rr.textureLibrary->GetSampler();
        VkImageView envMapView = envMapTex ? envMapTex->GetImageView() : VK_NULL_HANDLE;

        if (!envMapTex)
        {
            RADIS_ERROR("UpdateRTUniform: environment map not found");
            return;
        }

        auto makeInfo = [](VkImageLayout layout, VkSampler s, VkImageView view) -> VkDescriptorImageInfo
        {
            return { s, view, layout };
        };

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const int histIdx = (frame + frameCount - 1) % frameCount;

            auto outSceneHDR = makeInfo(VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE, sceneHDRTex->GetImageView());
            auto heatmap = makeInfo(VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE, rtHeatmapTextures[frame]->GetImageView());
            auto historyRead = makeInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler, rtAccumTextures[histIdx]->GetImageView());
            auto historyWrite = makeInfo(VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE, rtAccumTextures[frame]->GetImageView());
            auto envMap = makeInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler, envMapView);

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

    void TextureLibrary::ClearAllBuffers()
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

        for (auto& tex : mTextures)
            tex.reset();
    }

    void TextureLibrary::RecreateAllBuffers()
    {
        CreateTextureSampler();
        CreateDescriptors();

        for (uint32_t i = 0; i < mNextIndex; ++i)
        {
            const TextureData& td = mTexturesData[i];
            if (td.name.empty() && td.pixels.empty()) continue;

            VkImageLayout layout = (td.isStorageImage || td.isSpecialImage)
                ? td.finalLayout
                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            InstantiateTexture(i, layout);
        }
    }

    // =========================================================================
    //  Vulkan object creation (private)
    // =========================================================================

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
        {
            RADIS_CRITICAL("TextureLibrary: failed to create texture sampler");
        }
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

        if (vkCreateDescriptorSetLayout(device->GetDevice(), &layoutInfo, nullptr, &mImageDescriptorSetLayout) != VK_SUCCESS)
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

        if (vkCreateDescriptorPool(device->GetDevice(), &poolInfo, nullptr, &mImageDescriptorPool) != VK_SUCCESS)
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

        if (vkAllocateDescriptorSets(device->GetDevice(), &allocInfo, &texture->mDescriptorSet) != VK_SUCCESS)
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