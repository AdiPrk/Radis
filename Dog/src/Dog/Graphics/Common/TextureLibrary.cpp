#include <PCH/pch.h>
#include "TextureLibrary.h"

#include "../RHI/ITexture.h"
#include "../Vulkan/Texture/Texture.h"
#include "../Vulkan/Core/Device.h"
#include "../OpenGL/GLTexture.h"

#include "Engine.h"

namespace Dog
{
    const uint32_t TextureLibrary::MAX_TEXTURE_COUNT = 50;

    TextureLibrary::TextureLibrary(Device* device)
        : device{ device }
        , mTextureSampler{ VK_NULL_HANDLE }
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            CreateTextureSampler();
        }
    }

    TextureLibrary::~TextureLibrary()
    {
        if (mTextureSampler && device)
        {
            vkDestroySampler(device->GetDevice(), mTextureSampler, nullptr);
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

        // Load new texture
        std::unique_ptr<ITexture> newTexture;
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) 
        {
            newTexture = std::make_unique<VKTexture>(*device, filePath);
        }
        else
        {
            newTexture = std::make_unique<GLTexture>(filePath);
        }

        uint32_t newIndex = static_cast<uint32_t>(mTextures.size());
        mTextures.push_back(std::move(newTexture));
        mTextureMap[filePath] = newIndex;
        
        return newIndex;
    }

    uint32_t TextureLibrary::AddTexture(const unsigned char* textureData, uint32_t textureSize, const std::string& texturePath)
    {
        // Check if texture already exists
        auto it = mTextureMap.find(texturePath);
        if (it != mTextureMap.end()) {
            return it->second; // Return existing texture index
        }

        // Load new texture
        std::unique_ptr<ITexture> newTexture;
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            newTexture = std::make_unique<VKTexture>(*device, texturePath, textureData, textureSize);
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            newTexture = std::make_unique<GLTexture>(texturePath, textureData, textureSize);
        }

        uint32_t newIndex = static_cast<uint32_t>(mTextures.size());
        mTextures.push_back(std::move(newTexture));
        mTextureMap[texturePath] = newIndex;

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

    void TextureLibrary::ClearAllBuffers(class Device* device)
    {
        for (auto& texture : mTextures)
        {
            ITexture* itex = texture.get();
         
            auto& pixelData = mCpuPixelArrays.emplace_back();
            pixelData.data = std::move(itex->mUncompressedData.data);
            pixelData.width = itex->mWidth;
            pixelData.height = itex->mHeight;
            pixelData.channels = itex->mChannels;
            pixelData.path = itex->mPath;
        }

        if (mTextureSampler && device)
        {
            vkDestroySampler(device->GetDevice(), mTextureSampler, nullptr);
            mTextureSampler = VK_NULL_HANDLE;
        }

        mTextures.clear();
        mTextureMap.clear();
    }

    void TextureLibrary::RecreateAllBuffers(class Device* device)
    {
        for (auto& pixelData : mCpuPixelArrays)
        {
            if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
            {
                mTextures.emplace_back(std::make_unique<VKTexture>(*device, pixelData));
            }
            else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
            {
                mTextures.emplace_back(std::make_unique<GLTexture>(pixelData));
            }
            mTextureMap[pixelData.path] = static_cast<uint32_t>(mTextures.size() - 1);
        }

        mCpuPixelArrays.clear();
    }
}
