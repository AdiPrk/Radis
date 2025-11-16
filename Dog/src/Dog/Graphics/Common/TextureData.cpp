#include <PCH/pch.h>
#include "TextureData.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Dog
{
    bool TextureLoader::FromFile(const std::string& path, TextureData& outTexture)
    {
        if (!std::filesystem::exists(path))
        {
            DOG_ERROR("Texture file not found: {0}", path);
            return false;
        }

        stbi_set_flip_vertically_on_load(true);
        int width, height;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &outTexture.channels, STBI_rgb_alpha);
        if (!data)
        {
            DOG_ERROR("Failed to load texture: {0}", path);
            return false;
        }

        outTexture.channels = 4; // we force RGBA
        outTexture.width = width;
        outTexture.height = height;
        outTexture.pixels.assign(data, data + width * height * outTexture.channels);
        outTexture.name = path;

        stbi_image_free(data);
        return true;
    }

    bool TextureLoader::FromMemory(const unsigned char* textureData, uint32_t textureSize, const std::string& name, TextureData& outTexture)
    {
        if (!textureData || textureSize == 0)
        {
            DOG_ERROR("Invalid texture data provided");
            return false;
        }

        stbi_set_flip_vertically_on_load(true);
        int width, height, channels;
        unsigned char* data = stbi_load_from_memory(textureData, textureSize, &width, &height, &channels, STBI_rgb_alpha);
        if (!data)
        {
            DOG_ERROR("Failed to load texture from memory");
            return false;
        }

        outTexture.channels = 4; // we force RGBA
        outTexture.width = width;
        outTexture.height = height;
        outTexture.pixels.assign(data, data + width * height * outTexture.channels);
        outTexture.name = name;

        stbi_image_free(data);
        return true;
    }
}
