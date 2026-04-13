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

    class TextureLibrary
    {
    public:
        TextureLibrary(Device* device);
        ~TextureLibrary();

        TextureLibrary(const TextureLibrary&) = delete;
        TextureLibrary& operator=(const TextureLibrary&) = delete;

        uint32_t QueueTextureLoad(const std::string& texturePath);
        uint32_t QueueTextureLoad(const unsigned char* data, uint32_t size, const std::string& texturePath);

        /**
         * Decodes all queued textures on worker threads, then uploads them to the GPU. 
         *
         * \return Returns false when the pending queue is empty.
         */
        bool LoadQueuedTextures();

        // ---------------------------------------------------------------
        //  Immediate GPU texture creation
        // ---------------------------------------------------------------

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
        VkSampler GetSampler()      const { return mTextureSampler; }

        // ---------------------------------------------------------------
        //  Descriptor / uniform updates
        // ---------------------------------------------------------------

        void UpdateTextureUniform(Uniform* uniform);
        void UpdateRTUniform(RenderingResource& renderData);

        void ClearAllBuffers(class Device* device);
        void SetDevice(Device* dev) { device = dev; }

        // ---------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------

        static const uint32_t MAX_TEXTURE_COUNT;
        static const uint32_t INVALID_TEXTURE_INDEX;

    private:
        // ---------------------------------------------------------------
        //  Internal helpers
        // ---------------------------------------------------------------
        uint32_t AllocateSlot(const std::string& name);
        void InstantiateTexture(uint32_t index, VkImageLayout layout);
        void RecreateAtIndex(uint32_t index, uint32_t newWidth, uint32_t newHeight);

        void CreateTextureSampler();
        void CreateDescriptors();
        void CreateDescriptorSet(VKTexture* texture, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // ---------------------------------------------------------------
        //  Data
        // ---------------------------------------------------------------

        std::vector<std::unique_ptr<ITexture>> mTextures;
        std::vector<TextureData>               mTexturesData;
        std::unordered_map<std::string, uint32_t> mTextureMap;

        std::vector<TextureLoadData> mPendingTextureLoads;

        Device* device = nullptr;
        VkSampler             mTextureSampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout mImageDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      mImageDescriptorPool = VK_NULL_HANDLE;

        uint32_t mNextIndex = 0;
        bool     mNeedReuploadRTImage = false;
        bool     mNeedTextureDescriptorUpdate = false;
    };

} // namespace Radis