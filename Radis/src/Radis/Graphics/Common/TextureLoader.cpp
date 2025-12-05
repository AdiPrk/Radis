#include <PCH/pch.h>
#include "TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ktx2
#include "ktx.h"

namespace Radis
{
    bool TextureLoader::IsKTX2Path(const std::string& path)
    {
        return path.size() >= 5 && path.substr(path.size() - 5) == ".ktx2";
    }

    void TextureLoader::CreateKTX2File(const std::string& path, const std::string& outputPath)
    {
        std::string toktxPath = Assets::BinariesPath + "toktx.exe";
        std::string inner =
            "\"" + toktxPath + "\" "
            "--t2 --encode etc1s --genmipmap --qlevel 128 --clevel 1 --assign_oetf srgb --lower_left_maps_to_s0t0 "
            "\"" + outputPath + "\" "
            "\"" + path + "\"";

        std::string command = "\"" + inner + "\"";

        std::cout << command << std::endl;
        std::system(command.c_str());
    }

    void TextureLoader::BuildKTX2File(const KTX2BuildInput& input, const std::string& outPath)
    {
        if (outPath.empty())
            return;

        std::filesystem::path out(outPath);

        // If it already exists, don't rebuild
        if (std::filesystem::exists(out))
            return;

        // Ensure the directory exists
        std::filesystem::path parent = out.parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);

        std::string srcPath;
        std::filesystem::path tempSrcPath;
        bool hasTemp = false;

        if (!input.sourcePath.empty())
        {
            // Use the original source file
            srcPath = input.sourcePath;
        }
        else if (input.data && !input.data->empty())
        {
            static std::atomic<uint64_t> sTempCounter{ 0 };

            uint64_t id = sTempCounter.fetch_add(1, std::memory_order_relaxed);
            std::filesystem::path tempDir = parent.empty() ? std::filesystem::temp_directory_path() : parent;
            tempSrcPath = tempDir / ("ktx2_src_" + std::to_string(id));

            std::ofstream srcFile(tempSrcPath, std::ios::binary);
            if (!srcFile)
            {
                DOG_CRITICAL("Failed to create temp texture file: %s", tempSrcPath.string().c_str());
                return;
            }

            srcFile.write(reinterpret_cast<const char*>(input.data->data()),
                static_cast<std::streamsize>(input.data->size()));

            srcPath = tempSrcPath.string();
            hasTemp = true;
        }
        else
        {
            // No valid source
            return;
        }

        // Use existing KTX2 creator
        TextureLoader::CreateKTX2File(srcPath, outPath);

        if (hasTemp)
        {
            std::error_code ec;
            std::filesystem::remove(tempSrcPath, ec);
        }
    }

    bool TextureLoader::FromFile(const std::string& path, TextureData& outTexture)
    {
        if (!std::filesystem::exists(path))
        {
            DOG_ERROR("Texture file not found: {0}", path);
            return false;
        }

        if (IsKTX2Path(path))
        {
            return FromKTX2File(path, outTexture);
        }
        else
        {
            return FromSTBFile(path, outTexture);
        }
    }

    bool TextureLoader::FromKTX2File(const std::string& path, TextureData& outTexture)
    {
        // Load texture from file into memory
        ktxTexture2* kTexture = nullptr;
        KTX_error_code result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);

        if (result != KTX_SUCCESS || !kTexture)
        {
            DOG_ERROR("Failed to load KTX2 texture: {} (error {})", path, (int)result);
            return false;
        }

        // Transcode ETC1S+KTX2 to BC7
        if (ktxTexture2_NeedsTranscoding(kTexture))
        {
            result = ktxTexture2_TranscodeBasis(kTexture, KTX_TTF_BC7_RGBA, 0);

            if (result != KTX_SUCCESS)
            {
                DOG_ERROR("Failed to transcode KTX2 texture: {} (error {})", path, (int)result);
                ktxTexture_Destroy(ktxTexture(kTexture));
                return false;
            }
        }

        // Basic info
        outTexture.width = static_cast<int>(kTexture->baseWidth);
        outTexture.height = static_cast<int>(kTexture->baseHeight);
        outTexture.channels = 4; // logical RGBA
        outTexture.name = path;

        outTexture.isCompressed = true;
        outTexture.mipLevels = kTexture->numLevels;
        outTexture.mipInfos.clear();
        outTexture.mipInfos.reserve(outTexture.mipLevels);

        // Vulkan compressed format matching BC7 RGBA sRGB
        outTexture.imageFormat = VK_FORMAT_BC7_SRGB_BLOCK;

        // Copy the entire transcode buffer
        const ktx_size_t totalSize = kTexture->dataSize;
        outTexture.pixels.resize(static_cast<size_t>(totalSize));
        std::memcpy(outTexture.pixels.data(), kTexture->pData, static_cast<size_t>(totalSize));

        // Build mipInfos: offset & size for each mip level
        for (uint32_t level = 0; level < outTexture.mipLevels; ++level)
        {
            ktx_size_t offset = 0;
            result = ktxTexture_GetImageOffset(ktxTexture(kTexture), level, 0, 0, &offset);

            if (result != KTX_SUCCESS)
            {
                DOG_ERROR("Failed to get image offset for KTX2 texture: {} (level {}, error {})", path, level, (int)result);
                ktxTexture_Destroy(ktxTexture(kTexture));
                return false;
            }

            const ktx_size_t levelSize = ktxTexture_GetImageSize(ktxTexture(kTexture), level);

            TextureData::MipLevelInfo mip{};
            mip.width = std::max(1u, kTexture->baseWidth >> level);
            mip.height = std::max(1u, kTexture->baseHeight >> level);
            mip.offset = static_cast<size_t>(offset);
            mip.size = static_cast<size_t>(levelSize);

            outTexture.mipInfos.push_back(mip);
        }

        ktxTexture_Destroy(ktxTexture(kTexture));
        return true;
    }

    bool TextureLoader::FromSTBFile(const std::string& path, TextureData& outTexture)
    {
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
        outTexture.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
        outTexture.isCompressed = false;
        outTexture.mipLevels = 1;
        outTexture.mipInfos.clear();

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
        outTexture.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
        outTexture.isCompressed = false;
        outTexture.mipLevels = 1;
        outTexture.mipInfos.clear();

        stbi_image_free(data);
        return true;
    }

    void TextureLoader::LoadMT(std::vector<TextureLoadData>& loadData)
    {
        if (loadData.empty()) return;

        std::vector<std::future<bool>> futures;
        futures.reserve(loadData.size());

        for (auto& entry : loadData)
        {
            futures.emplace_back(std::async(std::launch::async, [&entry]()
            {
                if (entry.data != nullptr && entry.size > 0)
                {
                    return TextureLoader::FromMemory(entry.data, entry.size, entry.path, entry.outTexture);
                }
                else if (!entry.path.empty())
                {
                    return TextureLoader::FromFile(entry.path, entry.outTexture);
                }

                return false;
            }));
        }

        // Wait for all tasks to complete
        for (auto& f : futures)
        {
            f.get();
        }
    }
}
