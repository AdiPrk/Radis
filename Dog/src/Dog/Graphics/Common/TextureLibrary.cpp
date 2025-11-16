#include <PCH/pch.h>
#include "TextureLibrary.h"

#include "../RHI/ITexture.h"
#include "../Vulkan/Texture/VKTexture.h"
#include "../Vulkan/Core/Device.h"
#include "../OpenGL/GLTexture.h"

#include "TextureData.h"
#include "Engine.h"

namespace Dog
{
    const uint32_t TextureLibrary::MAX_TEXTURE_COUNT = 500;
    const uint32_t TextureLibrary::INVALID_TEXTURE_INDEX = 10001;

    TextureLibrary::TextureLibrary(Device* device)
        : device{ device }
        , mTextureSampler{ VK_NULL_HANDLE }
    {
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

    uint32_t TextureLibrary::AddTexture(const std::string& filePath)
    {
        // Check if texture already exists
        auto it = mTextureMap.find(filePath);
        if (it != mTextureMap.end()) {
            return it->second; // Return existing texture index
        }

        auto& textureData = mTexturesData.emplace_back();
        if (!TextureLoader::FromFile(filePath, textureData))
        {
            mTexturesData.pop_back();
            return INVALID_TEXTURE_INDEX;
        }

        // Load new texture
        std::unique_ptr<ITexture> newTexture;
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) 
        {
            textureData.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
            newTexture = std::make_unique<VKTexture>(*device, textureData);
            CreateDescriptorSet(static_cast<VKTexture*>(newTexture.get()));
        }
        else
        {
            newTexture = std::make_unique<GLTexture>(textureData);
        }

        uint32_t newIndex = static_cast<uint32_t>(mTextures.size());
        mTextures.push_back(std::move(newTexture));
        mTextureMap[filePath] = newIndex;

        DOG_INFO("Loaded texture: {0} (Index: {1})", filePath, newIndex);
        
        return newIndex;
    }

    uint32_t TextureLibrary::AddTexture(const unsigned char* data, uint32_t size, const std::string& texturePath)
    {
        // Check if texture already exists
        auto it = mTextureMap.find(texturePath);
        if (it != mTextureMap.end()) {
            return it->second; // Return existing texture index
        }

        auto& textureData = mTexturesData.emplace_back();
        if (!TextureLoader::FromMemory(data, size, texturePath, textureData))
        {
            mTexturesData.pop_back();
            return INVALID_TEXTURE_INDEX;
        }

        // Load new texture
        std::unique_ptr<ITexture> newTexture;
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            textureData.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
            newTexture = std::make_unique<VKTexture>(*device, textureData);
            CreateDescriptorSet(static_cast<VKTexture*>(newTexture.get()));
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            newTexture = std::make_unique<GLTexture>(textureData);
        }

        uint32_t newIndex = static_cast<uint32_t>(mTextures.size());
        mTextures.push_back(std::move(newTexture));
        mTextureMap[texturePath] = newIndex;

        DOG_INFO("Loaded texture from memory: {0} (Index: {1})", texturePath, newIndex);

        return newIndex;
    }

    uint32_t TextureLibrary::CreateImage(const std::string& imageName, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageUsageFlags usage, VkImageLayout finalLayout)
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            DOG_ERROR("CreateImage called for non-Vulkan API");
            return INVALID_TEXTURE_INDEX;
        }

        // check if exists
        auto it = mTextureMap.find(imageName);
        if (it != mTextureMap.end())
        {
            return it->second;
        }

        auto& textureData = mTexturesData.emplace_back();
        textureData.isStorageImage = true;
        textureData.width = width;
        textureData.height = height;
        textureData.imageFormat = imageFormat;
        textureData.usage = usage;
        textureData.finalLayout = finalLayout;

        std::unique_ptr<ITexture> newTexture;
        newTexture = std::make_unique<VKTexture>(*device, textureData);

        uint32_t newIndex = static_cast<uint32_t>(mTextures.size());
        mTextures.push_back(std::move(newTexture));
        mTextureMap[imageName] = newIndex;
        CreateDescriptorSet(static_cast<VKTexture*>(mTextures.back().get()), finalLayout);
        return newIndex;
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

        DOG_ERROR("Texture index out of range: {0}", index);
        return mTextures[0].get();
    }

    void TextureLibrary::CreateTextureSampler()
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            //DOG_WARN("CreateTextureSampler called for non-Vulkan API");
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

        if (vkCreateSampler(device->GetDevice(), &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
        {
            DOG_CRITICAL("Failed to create texture sampler");
        }
    }

    void TextureLibrary::CreateDescriptors()
    {
        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            //DOG_WARN("CreateTextureSampler called for non-Vulkan API");
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
        mTextureMap.clear();
    }

    void TextureLibrary::RecreateAllBuffers(class Device* device)
    {
        for (auto& textureData : mTexturesData)
        {
            if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
            {
                textureData.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
                if (textureData.isStorageImage)
                {
                    textureData.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
                }

                mTextures.emplace_back(std::make_unique<VKTexture>(*device, textureData));
                CreateDescriptorSet(static_cast<VKTexture*>(mTextures.back().get()));
            }
            else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
            {
                mTextures.emplace_back(std::make_unique<GLTexture>(textureData));
            }

            mTextureMap[textureData.name] = static_cast<uint32_t>(mTextures.size() - 1);
        }
    }
}
