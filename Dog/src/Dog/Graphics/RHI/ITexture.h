#pragma once

namespace Dog
{
    struct TextureData
    {
        int width{};
        int height{};
        int channels{};
        std::string name{};
        std::vector<unsigned char> pixels{};

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
