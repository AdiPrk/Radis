/*****************************************************************//**
 * \file   GLTexture.cpp
 * \brief  Implementation of the GLTexture class for OpenGL texture management.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "GLTexture.h"

namespace Radis {

    GLuint GLTexture::CurrentTextureID = 0;

    GLTexture::GLTexture(const TextureData& textureData)
        : ITexture(textureData)
        , Rows(1), Columns(1), Index(0)
        , IsSpriteSheet(false)
        , Internal_Format(GL_SRGB8_ALPHA8)
        , Image_Format(GL_RGBA)
        , Wrap_S(GL_REPEAT), Wrap_T(GL_REPEAT)
        , Filter_Min(GL_LINEAR_MIPMAP_LINEAR)
        , Filter_Max(GL_LINEAR)
        , textureHandle(0)
        , SpriteWidth(0), SpriteHeight(0)
        , NumSprites(1), ID(0)
    {
        if (textureData.isStorageImage) return;

        glGenTextures(1, &this->ID);

        if (textureData.isHDR)
        {
            Internal_Format = GL_RGBA16F;
            Image_Format = GL_RGBA;
            Filter_Min = GL_LINEAR;
        }
        else if (textureData.isCompressed)
        {
            Internal_Format = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
            Image_Format = GL_RGBA;
            Filter_Min = GL_LINEAR_MIPMAP_LINEAR;
        }
        else
        {
            Internal_Format = GL_SRGB8_ALPHA8;
            Image_Format = GL_RGBA;
            Filter_Min = GL_LINEAR_MIPMAP_LINEAR;
        }

        SpriteWidth = textureData.width;
        SpriteHeight = textureData.height;

        Generate(textureData.width, textureData.height);
    }

    GLTexture::~GLTexture()
    {
        glDeleteTextures(1, &this->ID);
    }

    void GLTexture::Generate(unsigned int width, unsigned int height)
    {
        glBindTexture(GL_TEXTURE_2D, this->ID);

        if (mData.isHDR)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, Internal_Format,
                width, height, 0,
                Image_Format, GL_FLOAT,
                mData.floatPixels.data());
            // HDR textures typically want manual mip generation or none at all
            // glGenerateMipmap here is fine if you want it
        }
        else if (mData.isCompressed)
        {
            for (uint32_t level = 0; level < mData.mipLevels; ++level)
            {
                const auto& mip = mData.mipInfos[level];
                glCompressedTexImage2D(
                    GL_TEXTURE_2D,
                    static_cast<GLint>(level),
                    Internal_Format,
                    static_cast<GLsizei>(mip.width),
                    static_cast<GLsizei>(mip.height),
                    0,
                    static_cast<GLsizei>(mip.size),
                    mData.pixels.data() + mip.offset
                );
            }
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, Internal_Format,
                width, height, 0,
                Image_Format, GL_UNSIGNED_BYTE,
                mData.pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, Wrap_S);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, Wrap_T);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Filter_Min);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Filter_Max);

        textureHandle = glGetTextureHandleARB(this->ID);
        glMakeTextureHandleResidentARB(textureHandle);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void GLTexture::Bind() const
    {
        if (this->ID != CurrentTextureID) {
            glBindTexture(GL_TEXTURE_2D, this->ID);
            CurrentTextureID = this->ID;
        }
    }

}