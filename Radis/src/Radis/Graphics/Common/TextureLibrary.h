/*****************************************************************//**
 * \file   TextureLibrary.h
 * \brief  Definition of the TextureLibrary class for managing textures.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Graphics/RHI/ITexture.h"
#include "TextureLoader.h"

namespace Radis
{
    class Device;
    class ITexture;
    class VKTexture;
    class Uniform;
    struct RenderingResource;
    class ThreadPool;

    // Controls whether QueueTextureLoad blocks the calling thread.
    enum class LoadPriority
    {
        Async,
        Immediate,
    };

    class TextureLibrary
    {
    public:
        TextureLibrary(Device* device, ThreadPool& threadPool);
        ~TextureLibrary();

        TextureLibrary(const TextureLibrary&) = delete;
        TextureLibrary& operator=(const TextureLibrary&) = delete;

        uint32_t QueueTextureLoad(const std::string& texturePath, LoadPriority priority = LoadPriority::Async);
        uint32_t QueueTextureLoad(const unsigned char* data, uint32_t size, const std::string& textureName, LoadPriority priority = LoadPriority::Async);

        bool LoadQueuedTextures();
        void FlushAll();

        uint32_t CreateStorageImage(const std::string& name, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL);
        uint32_t CreateTexture(const std::string& name, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout finalLayout);

        void ResizeStorageImage(const std::string& name, uint32_t newWidth, uint32_t newHeight);
        void ResizeTexture(const std::string& name, uint32_t newWidth, uint32_t newHeight);

        // ---------------------------------------------------------------
        //  Accessors
        // ---------------------------------------------------------------

        ITexture* GetTexture(uint32_t id) const;
        ITexture* GetTexture(const std::string& path) const;
        VKTexture* GetVKTexture(const std::string& path) const;
        ITexture* GetTextureByIndex(uint32_t index) const;

        uint32_t  GetTextureCount() const { return mNextIndex; }
        VkSampler GetSampler() const { return mTextureSampler; }

        void UpdateTextureUniform(Uniform* uniform);
        void UpdateRTUniform(RenderingResource& renderData);

        void ClearAllBuffers();
        void RecreateAllBuffers();
        void SetDevice(Device* dev) { device = dev; }

        static const uint32_t MAX_TEXTURE_COUNT;
        static const uint32_t INVALID_TEXTURE_INDEX;

    private:
        // A texture that has finished CPU decoding and is waiting for GPU upload.
        struct DecodedTexture
        {
            uint32_t targetIndex = UINT32_MAX;
            TextureData data;
            std::string path;
        };

        // Insert (name -> slot) into mTextureMap and bump mNextIndex. Returns the existing index if the name is already known.
        uint32_t AllocateSlot(const std::string& name);

        // Build the VKTexture + descriptor set for slot `index` from mTexturesData[index].
        void InstantiateTexture(uint32_t index, VkImageLayout layout);

        // Update dimensions in mTexturesData[index], destroy the old GPU resource, recreate.
        void RecreateAtIndex(uint32_t index, uint32_t newWidth, uint32_t newHeight);

        // Upload a batch of decoded textures to the GPU (main thread only).
        void UploadBatch(std::vector<DecodedTexture>&& batch);

        void CreateTextureSampler();
        void CreateDescriptors();
        void CreateDescriptorSet(VKTexture* texture, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        std::vector<std::unique_ptr<ITexture>> mTextures;
        std::vector<TextureData> mTexturesData;
        std::unordered_map<std::string, uint32_t> mTextureMap;

        std::mutex mReadyMutex;
        std::vector<DecodedTexture> mReadyForGPU;

        ThreadPool* mThreadPool = nullptr;
        Device* device = nullptr;

        VkSampler mTextureSampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout mImageDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool mImageDescriptorPool = VK_NULL_HANDLE;

        uint32_t mNextIndex = 0;
        bool mNeedReuploadRTImage = false;
        bool mNeedTextureDescriptorUpdate = false;
    };

} // namespace Radis