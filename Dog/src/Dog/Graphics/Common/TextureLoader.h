#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Dog
{
    struct TextureLoadData
    {
        std::string path{};
        const unsigned char* data{ nullptr };
        uint32_t size{ 0 };
        TextureData outTexture{};
        uint32_t targetIndex{ 0 }; // For texture library
    };

    class TextureLoader
    {
    public:
        // Single threaded
        static bool FromFile(const std::string& path, TextureData& outTexture);
        static bool FromKTX2File(const std::string& path, TextureData& outTexture);
        static bool FromSTBFile(const std::string& path, TextureData& outTexture);
        static bool FromMemory(const unsigned char* textureData, uint32_t textureSize, const std::string& name, TextureData& outTexture);

        // Multi-threaded
        static void LoadMT(std::vector<TextureLoadData>& loadData);
    };
}
