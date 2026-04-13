/*****************************************************************//**
 * \file   ITexture.h
 * \brief  Abstract texture interface and texture data structures
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/


#pragma once

namespace Radis
{
    /**
     * \brief Texture data container supporting both STB and KTX2 formats.
     *
     * For STB: isCompressed = false, mipLevels = 1, mipInfos empty.
     * For KTX2: isCompressed = true, mipLevels = N, mipInfos populated.
     */
    struct TextureData
    {
        int width{};
        int height{};
        int channels{};
        std::string name{};

        // LDR pixel data (STB/KTX2)
        std::vector<unsigned char> pixels{};

        // HDR pixel data, populated instead of pixels if isHDR = true
        std::vector<float> floatPixels{};
        bool isHDR{ false };

        // Compression & mips (for KTX2)
        bool isCompressed{ false };
        uint32_t mipLevels{ 1 };

        struct MipLevelInfo
        {
            uint32_t width{};
            uint32_t height{};
            size_t   offset{}; // byte offset into pixels[]
            size_t   size{};   // byte size of this mip
        };
        std::vector<MipLevelInfo> mipInfos{};

        // For storage images (or other special images)
        bool isStorageImage{ false };
        bool isSpecialImage{ false };
        VkFormat imageFormat{};
        VkImageUsageFlags usage{};
        VkImageLayout finalLayout{};
        VkImageTiling tiling{};

        // Helpers
        size_t ByteSize() const
        {
            return isHDR
                ? floatPixels.size() * sizeof(float)
                : pixels.size();
        }

        const void* RawData() const
        {
            return isHDR
                ? static_cast<const void*>(floatPixels.data())
                : static_cast<const void*>(pixels.data());
        }
    };

    class ITexture
    {
    public:
        ITexture(const TextureData& data);
        virtual ~ITexture();

        int GetWidth() const { return mData.width; }
        int GetHeight() const { return mData.height; }
        int GetChannels() const { return mData.channels; }
        size_t GetByteSize() const { return mData.ByteSize(); }

        virtual void* GetTextureID() = 0;

        const TextureData& mData;
    };
}
