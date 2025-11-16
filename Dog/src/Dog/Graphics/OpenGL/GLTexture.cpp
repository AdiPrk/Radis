#include <PCH/pch.h>
#include "GLTexture.h"
#include "Graphics/Common/TextureData.h"

namespace Dog {

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

        Internal_Format = GL_SRGB8_ALPHA8;
        Image_Format = GL_RGBA;

        // Calculate the width and height of a single sprite
        unsigned columns = 1, rows = 1;

        SpriteWidth = textureData.width / columns;
        SpriteHeight = textureData.height / rows;
        Rows = rows;
        Columns = columns;
        Index = 0;
        IsSpriteSheet = columns != 1 || rows != 1;

        Generate(textureData.width, textureData.height, textureData.pixels.data(), rows * columns);
    }

    GLTexture::~GLTexture()
    {
        glDeleteTextures(1, &this->ID);
    }

    void GLTexture::Generate(unsigned int width, unsigned int height, const unsigned char* data, unsigned int numSprites)
    {
        this->NumSprites = numSprites;

        // create Texture
        if (numSprites == 1) {
            glBindTexture(GL_TEXTURE_2D, this->ID);

            glTexImage2D(GL_TEXTURE_2D, 0, this->Internal_Format, width, height, 0, this->Image_Format, GL_UNSIGNED_BYTE, data);

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

            glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

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