#pragma once

namespace Dog
{
    /*
        for stb: isCompressed = false, mipLevels = 1, mipInfos can stay empty.
        for ktx2: isCompressed = true, mipLevels = kTexture->numLevels, fill mipInfos and pixels with BC7 blocks.
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
