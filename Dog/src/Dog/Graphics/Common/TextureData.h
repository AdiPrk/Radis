#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Dog
{
    class TextureLoader
    {
    public:
        static bool FromFile(const std::string& path, TextureData& outTexture);
        static bool FromMemory(const unsigned char* textureData, uint32_t textureSize, const std::string& name, TextureData& outTexture);
    };
}
