#include <PCH/pch.h>
#include "GLTexture.h"

#include "stb_image.h"

namespace Dog {

    GLuint GLTexture::CurrentTextureID = 0;

    bool GLTexture::load(const std::string& file)
    {
        stbi_set_flip_vertically_on_load(true);

        // check if file exists
        std::ifstream infile(file);
        if (!infile.good())
        {
            std::cerr << "Error: File does not exist: " << file << std::endl;
            return false;
        }
        infile.close();

        // load image
        int width, height;
        unsigned char* data = stbi_load(file.c_str(), &width, &height, &mChannels, STBI_rgb_alpha);
        mChannels = 4; // because we forced STBI_rgb_alpha

        // check if data is loaded successfully
        if (!data)
        {
            std::cerr << "Error: Failed to load image: " << file << std::endl;
            return false;
        }

        // Set the internal and image formats based on the number of channels
        if (mChannels == 4)
        {
            Internal_Format = GL_SRGB8_ALPHA8;
            Image_Format = GL_RGBA;
        }
        else if (mChannels == 3)
        {
            Internal_Format = GL_SRGB8;
            Image_Format = GL_RGB;
        }
        else
        {
            DOG_CRITICAL("Unsupported number of channels: {0}", mChannels);
            stbi_image_free(data);
            return false;
        }

        mUncompressedData.data.resize(width * height * mChannels);
        memcpy(mUncompressedData.data.data(), data, mUncompressedData.data.size());

        // Calculate the width and height of a single sprite
        unsigned columns = 1, rows = 1;

        SpriteWidth = width / columns;
        SpriteHeight = height / rows;
        Rows = rows;
        Columns = columns;
        Index = 0;
        IsSpriteSheet = columns != 1 || rows != 1;

        Generate(width, height, data, rows * columns);

        // and finally free image data
        stbi_image_free(data);

        return true;
    }

    bool GLTexture::loadFromData(const std::vector<char>& data)
    {
        // load image
        int width, height;
        unsigned char* imageData = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()), (int)data.size(), &width, &height, &mChannels, STBI_rgb_alpha);
        mChannels = 4; // because we forced STBI_rgb_alpha

        // check if data is loaded successfully
        if (!imageData)
        {
            std::cerr << "Error: Failed to load image from data." << std::endl;
            return false;
        }

        // Set the internal and image formats based on the number of channels
        Internal_Format = GL_SRGB8_ALPHA8;
        Image_Format = GL_RGBA;

        mUncompressedData.data.resize(width * height * mChannels);
        memcpy(mUncompressedData.data.data(), imageData, mUncompressedData.data.size());

        // Calculate the width and height of a single sprite
        unsigned columns = 1, rows = 1;

        SpriteWidth = width / columns;
        SpriteHeight = height / rows;
        Rows = rows;
        Columns = columns;
        Index = 0;
        IsSpriteSheet = columns != 1 || rows != 1;

        Generate(width, height, imageData, rows * columns);

        // and finally free image data
        stbi_image_free(imageData);

        return true;
    }

    GLTexture::GLTexture(const std::string& filePath)
        : ITexture()
        , Rows(1)
        , Columns(1)
        , Index(0)
        , IsSpriteSheet(false)
        , Internal_Format(GL_SRGB8)
        , Image_Format(GL_RGB)
        , Wrap_S(GL_REPEAT)
        , Wrap_T(GL_REPEAT)
        , Filter_Min(GL_LINEAR)
        , Filter_Max(GL_LINEAR)
        , textureHandle()
        , SpriteWidth(0)
        , SpriteHeight(0)
        , NumSprites(1)
        , ID(0)
    {
        mPath = filePath;

        unsigned int id;
        glGenTextures(1, &id);
        this->ID = id;

        load(filePath);
    }

    GLTexture::GLTexture(const std::string& filePath, const unsigned char* textureData, uint32_t textureSize)
        : ITexture()
        , Rows(1)
        , Columns(1)
        , Index(0)
        , IsSpriteSheet(false)
        , Internal_Format(GL_SRGB8)
        , Image_Format(GL_RGB)
        , Wrap_S(GL_REPEAT)
        , Wrap_T(GL_REPEAT)
        , Filter_Min(GL_LINEAR)
        , Filter_Max(GL_LINEAR)
        , textureHandle()
        , SpriteWidth(0)
        , SpriteHeight(0)
        , NumSprites(1)
        , ID(0)
    {
        mPath = filePath;

        unsigned int id;
        glGenTextures(1, &id);
        this->ID = id;

        std::vector<char> data(reinterpret_cast<const char*>(textureData),
                               reinterpret_cast<const char*>(textureData) + textureSize);

        loadFromData(data);
    }

    GLTexture::GLTexture(const UncompressedPixelData& data)
        : ITexture()
        , Rows(1)
        , Columns(1)
        , Index(0)
        , IsSpriteSheet(false)
        , Internal_Format(GL_SRGB8)
        , Image_Format(GL_RGB)
        , Wrap_S(GL_REPEAT)
        , Wrap_T(GL_REPEAT)
        , Filter_Min(GL_LINEAR)
        , Filter_Max(GL_LINEAR)
        , textureHandle()
        , SpriteWidth(0)
        , SpriteHeight(0)
        , NumSprites(1)
        , ID(0)
    {
        mUncompressedData = data;

        Internal_Format = GL_SRGB8_ALPHA8;
        Image_Format = GL_RGBA;

        unsigned columns = 1, rows = 1;
        SpriteWidth = data.width / columns;
        SpriteHeight = data.height / rows;
        Rows = rows;
        Columns = columns;
        Index = 0;
        IsSpriteSheet = columns != 1 || rows != 1;
        mPath = data.path;
        mWidth = data.width;
        mHeight = data.height;

        // no need for opengl to do if vk storage image;
        if (data.isStorageImage)
        {
            mStorageImage = true;
            return;
        }

        unsigned int id;
        glGenTextures(1, &id);
        this->ID = id;
        
        Generate(data.width, data.height, data.data.data(), rows * columns);
    }

    GLTexture::~GLTexture()
    {
        glDeleteTextures(1, &this->ID);
    }

    void GLTexture::Generate(unsigned int width, unsigned int height, const unsigned char* data, unsigned int numSprites)
    {
        this->mWidth = width;
        this->mHeight = height;
        this->NumSprites = numSprites;

        // create Texture
        if (numSprites == 1) {
            glBindTexture(GL_TEXTURE_2D, this->ID);

            glTexImage2D(GL_TEXTURE_2D, 0, this->Internal_Format, mWidth, mHeight, 0, this->Image_Format, GL_UNSIGNED_BYTE, data);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, this->Wrap_S);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, this->Wrap_T);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->Filter_Min);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->Filter_Max);
        }
        else {
            // glBindTexture(GL_TEXTURE_2D_ARRAY, this->ID);
            // glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGB8, width, height, 1);
            // glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, 1, Internal_Format, GL_UNSIGNED_BYTE, data);

            glBindTexture(GL_TEXTURE_2D_ARRAY, this->ID);

            // rgba8 works, rgba doesn't, ?
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, SpriteWidth, SpriteHeight, NumSprites);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, mWidth);

            for (int i = 0; i < static_cast<int>(NumSprites); ++i)
            {
                int xOffSet = i % Columns * SpriteWidth;
                int yOffSet = (i / Columns) * SpriteHeight;
                glPixelStorei(GL_UNPACK_SKIP_PIXELS, xOffSet);
                glPixelStorei(GL_UNPACK_SKIP_ROWS, yOffSet);

                // printf("Index: %i, xOffset: %i, yOffset: %i\n", i, xOffSet, yOffSet);

                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, SpriteWidth, SpriteHeight, 1, Internal_Format, GL_UNSIGNED_BYTE, data);
            }

            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        // Generate a handle for the texture and make it resident
        textureHandle = glGetTextureHandleARB(this->ID);
        glMakeTextureHandleResidentARB(textureHandle);

        // Reset stuff
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }

    void GLTexture::Bind() const
    {
        if (this->ID != CurrentTextureID) {
            glBindTexture(GL_TEXTURE_2D, this->ID);
            CurrentTextureID = this->ID;
        }
    }

}