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

        // For storage images:
        bool isStorageImage{ false };
        VkFormat imageFormat{};
        VkImageUsageFlags usage{};
        VkImageLayout finalLayout{};
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

    protected:
        const TextureData& mData;
    };
}
