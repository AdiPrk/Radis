/*****************************************************************//**
 * \file   TextureLoader.h
 * \brief  Definition of the TextureLoader class for loading textures from files and memory.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Graphics/RHI/ITexture.h"
#include "ECS/Resources/Assets/AssetLoader.h"

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

    class TextureLoader : public AssetLoader<TextureData>
    {
    public:
        TextureLoader() {};
        ~TextureLoader() override {};

        std::shared_ptr<TextureData> Load(LoadContext& ctx, std::string& errorOut) override;
        void Finalize(class ECS* ecs, AssetID id, TextureData& asset) override;

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

    private:
        static void FillHDRTexture(TextureData& out, const std::string& name, float* data, int width, int height);
        static void FillLDRTexture(TextureData& out, const std::string& name, unsigned char* data, int width, int height);
    };
}
