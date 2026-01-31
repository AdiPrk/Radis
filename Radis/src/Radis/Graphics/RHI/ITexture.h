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
        std::vector<unsigned char> pixels{};

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
    };

    class ITexture
    {
    public:
        ITexture(const TextureData& data);
        virtual ~ITexture();

        int GetWidth() const { return mData.width; }
        int GetHeight() const { return mData.height; }
        int GetChannels() const { return mData.channels; }
        uint64_t GetImageSize() const { return mData.pixels.size(); }

        virtual void* GetTextureID() = 0;

        const TextureData& mData;
    };
}
