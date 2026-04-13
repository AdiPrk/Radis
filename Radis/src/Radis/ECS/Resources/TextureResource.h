/*****************************************************************//**
 * \file   TextureResource.h
 * \brief  Resource for managing the texture uniform
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "IResource.h"

namespace Radis
{
    struct RenderingResource;
    class ITexture;
    class Device;
    class VKTexture;

    struct TextureResource : public IResource
    {
        TextureResource(RenderingResource& rr);
        ~TextureResource();

        void Shutdown() override;

        void UpdateTextureUniform(uint32_t assetID, struct TextureData& asset);

        void CreateTextureSampler();
        void CreateDescriptors();
        void CreateDescriptorSet(VKTexture* texture, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        void SetDevice(Device* dev) { device = dev; }

    private:
        class Uniform* textureUniform;

        std::vector<std::unique_ptr<VKTexture>> mTextures{}; // Indexed by texture index, contains the actual VKTexture objects

        Device* device = nullptr;
        VkSampler mTextureSampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout mImageDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool mImageDescriptorPool = VK_NULL_HANDLE;

        
    };
}
