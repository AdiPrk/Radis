#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Radis 
{
    class GLTexture : public ITexture
    {
    public:
        GLTexture(const TextureData& textureData);
        ~GLTexture();

        GLTexture(const GLTexture&) = delete;
        GLTexture& operator=(const GLTexture&) = delete;

        void* GetTextureID() override { return reinterpret_cast<void*>(static_cast<uintptr_t>(ID)); }

        // generates texture from image data
        void Generate(unsigned int width, unsigned int height, const unsigned char* data);
        void Bind() const;

        // holds the ID of the texture object, used for all texture operations to reference to this particular texture
        unsigned int ID;
        // texture image dimensions
        unsigned int NumSprites; // number of sprites in spritesheet
        unsigned int SpriteWidth, SpriteHeight; // For spritesheets
        unsigned int Rows, Columns; // For spritesheets
        unsigned int Index; // Index for spritesheet
        bool IsSpriteSheet; // is it a spritesheet?
        // texture Format
        unsigned int Internal_Format; // format of texture object
        unsigned int Image_Format; // format of loaded image
        // texture configuration
        unsigned int Wrap_S; // wrapping mode on S axis
        unsigned int Wrap_T; // wrapping mode on T axis
        unsigned int Filter_Min; // filtering mode if texture pixels < screen pixels
        unsigned int Filter_Max; // filtering mode if texture pixels > screen pixels
        unsigned long long textureHandle; // texture handle for bindless textures

        // keeps track of the currently bound texture ID
        static unsigned int CurrentTextureID;
    };

}