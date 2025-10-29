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

    protected:
        int mWidth, mHeight, mChannels;
        uint64_t mImageSize;
        std::string mPath;

        struct UncompressedPixelData
        {
            int width, height, channels;
            std::vector<unsigned char> data;
            std::string path;
        };

        friend class TextureLibrary;
        UncompressedPixelData mUncompressedData;
    };
}
