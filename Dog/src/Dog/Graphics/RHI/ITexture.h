#pragma once

namespace Dog
{
    class ITexture
    {
    public:
        ITexture();
        virtual ~ITexture();

        int GetWidth() const { return mWidth; }
        int GetHeight() const { return mHeight; }
        int GetChannels() const { return mChannels; }
        uint64_t GetImageSize() const { return mImageSize; }

        virtual void* GetTextureID() = 0;

    protected:
        int mWidth, mHeight, mChannels;
        uint64_t mImageSize;
        std::string mPath;
        bool mStorageImage = false; // For certain vk textures

        struct UncompressedPixelData
        {
            int width, height, channels;
            std::vector<unsigned char> data;
            std::string path;

            // For storage images;
            bool isStorageImage = false;
            VkFormat imageFormat;
            VkImageUsageFlags usage;
            VkImageLayout finalLayout;
        };

        friend class TextureLibrary;
        UncompressedPixelData mUncompressedData;
    };
}
