#include <PCH/pch.h>
#include "TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ktx2
#include "ktx.h"

namespace Dog
{
    static bool IsKTX2Path(const std::string& path)
    {
        return path.size() >= 5 && path.substr(path.size() - 5) == ".ktx2";
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
        ktxTexture2* kTexture = nullptr;

        KTX_error_code result = ktxTexture2_CreateFromNamedFile(
            path.c_str(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, // load image data into memory
            &kTexture
        );

        if (result != KTX_SUCCESS || !kTexture)
        {
            DOG_ERROR("Failed to load KTX2 texture: {} (error {})", path, (int)result);
            return false;
        }

        // Your toktx pipeline produces ETC1S+KTX2 so transcoding is required.
        if (ktxTexture2_NeedsTranscoding(kTexture))
        {
            // Transcode to uncompressed RGBA32 (raw 4-byte-per-texel pixels).
            result = ktxTexture2_TranscodeBasis(
                kTexture,
                KTX_TTF_RGBA32, // <-- correct uncompressed RGBA target
                0               // no special flags
            );

            if (result != KTX_SUCCESS)
            {
                DOG_ERROR("Failed to transcode KTX2 texture: {} (error {})", path, (int)result);
                ktxTexture_Destroy(ktxTexture(kTexture));
                return false;
            }
        }

        // Use base mip level (0), first layer, first face
        const uint32_t level = 0;
        const uint32_t layer = 0;
        const uint32_t face = 0;

        ktx_size_t offset = 0;
        result = ktxTexture_GetImageOffset(
            ktxTexture(kTexture),
            level,
            layer,
            face,
            &offset
        );

        if (result != KTX_SUCCESS)
        {
            DOG_ERROR("Failed to get image offset for KTX2 texture: {} (error {})", path, (int)result);
            ktxTexture_Destroy(ktxTexture(kTexture));
            return false;
        }

        // ktxTexture_GetImageSize RETURNS the size, not via out-param.
        ktx_size_t imageSize = ktxTexture_GetImageSize(
            ktxTexture(kTexture),
            level
        );

        // Fill TextureData similarly to STB path
        outTexture.width = static_cast<int>(kTexture->baseWidth);
        outTexture.height = static_cast<int>(kTexture->baseHeight);
        outTexture.channels = 4;       // RGBA
        outTexture.name = path;

        outTexture.pixels.resize(static_cast<size_t>(imageSize));
        const auto* src = static_cast<const unsigned char*>(kTexture->pData) + offset;
        std::memcpy(outTexture.pixels.data(), src, static_cast<size_t>(imageSize));

        // sRGB because toktx used --assign_oetf srgb
        outTexture.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

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
