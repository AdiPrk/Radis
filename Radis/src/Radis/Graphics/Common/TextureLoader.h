#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Radis
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
        struct KTX2BuildInput
        {
            std::string sourcePath;                     // if original file exists
            const std::vector<unsigned char>* data = nullptr; // if embedded texture
        };

        // Build a KTX2 file on disk from a path OR in-memory data.
        static void BuildKTX2File(const KTX2BuildInput& input, const std::string& outPath);

        static bool FromFile(const std::string& path, TextureData& outTexture);
        static bool FromKTX2File(const std::string& path, TextureData& outTexture);
        static bool FromSTBFile(const std::string& path, TextureData& outTexture);
        static bool FromMemory(const unsigned char* textureData, uint32_t textureSize, const std::string& name, TextureData& outTexture);

        // Multi-threaded
        static void LoadMT(std::vector<TextureLoadData>& loadData);
        
        // Helpers
        static bool IsKTX2Path(const std::string& path);
        static void CreateKTX2File(const std::string& path, const std::string& outputPath);
    };
}
