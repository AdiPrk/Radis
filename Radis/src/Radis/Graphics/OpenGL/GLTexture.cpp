#include <PCH/pch.h>
#include "GLTexture.h"

namespace Radis {

    GLuint GLTexture::CurrentTextureID = 0;

    GLTexture::GLTexture(const TextureData& textureData)
        : ITexture(textureData)
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
        // Only for Vulkan storage images
        if (textureData.isStorageImage) return;

        unsigned int id;
        glGenTextures(1, &id);
        this->ID = id;

        if (textureData.isCompressed)
        {
            // BC7 sRGB
            Internal_Format = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
            Image_Format = GL_RGBA; // not used for compressed upload
            Filter_Min = GL_LINEAR_MIPMAP_LINEAR; // make use of mips
        }
        else
        {
            Internal_Format = GL_SRGB8_ALPHA8;
            Image_Format = GL_RGBA;
            Filter_Min = GL_LINEAR_MIPMAP_LINEAR; // or GL_LINEAR if you prefer
        }

        unsigned columns = 1, rows = 1;
        SpriteWidth  = textureData.width / columns;
        SpriteHeight = textureData.height / rows;
        Rows         = rows;
        Columns      = columns;
        Index        = 0;
        IsSpriteSheet = columns != 1 || rows != 1;

        Generate(textureData.width, textureData.height, textureData.pixels.data());
    }

    GLTexture::~GLTexture()
    {
        glDeleteTextures(1, &this->ID);
    }

    void GLTexture::Generate(unsigned int width, unsigned int height, const unsigned char* data)
    {
        glBindTexture(GL_TEXTURE_2D, this->ID);

        if (!mData.isCompressed)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, Internal_Format, width, height, 0, Image_Format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else
        {
            // Compressed path: upload all mip levels using mData.mipInfos
            for (uint32_t level = 0; level < mData.mipLevels; ++level)
            {
                const auto& mip = mData.mipInfos[level];
                const void* src = mData.pixels.data() + mip.offset;

                glCompressedTexImage2D(
                    GL_TEXTURE_2D,
                    static_cast<GLint>(level),
                    this->Internal_Format,   // GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
                    static_cast<GLsizei>(mip.width),
                    static_cast<GLsizei>(mip.height),
                    0,
                    static_cast<GLsizei>(mip.size),
                    src
                );
            }
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, this->Wrap_S);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, this->Wrap_T);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->Filter_Min);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->Filter_Max);

        textureHandle = glGetTextureHandleARB(this->ID);
        glMakeTextureHandleResidentARB(textureHandle);

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