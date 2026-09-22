#include <pch.h>
#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

static constexpr int kChannels = 4;

static void FreeMalloced(void* p) { std::free(p); }

static std::expected<SourceImage, std::string> LoadExr(const std::string& file)
{
    float* rgba = nullptr;
    int         width = 0;
    int         height = 0;
    const char* error = nullptr;

    // Loads the default layer as RGBA float; alpha is 1 if the file has none.
    if (LoadEXR(&rgba, &width, &height, file.c_str(), &error) != TINYEXR_SUCCESS)
    {
        std::string message = error ? error : "failed to load EXR";
        if (error) FreeEXRErrorMessage(error);
        return std::unexpected(message);
    }

    SourceImage image;
    image.width = uint32_t(width);
    image.height = uint32_t(height);
    image.type = PixelType::F32;
    image.pixels = PixelBuffer(rgba, &FreeMalloced); // tinyexr allocates with malloc
    return image;
}

static std::expected<SourceImage, std::string> LoadStb(const std::string& file)
{
    // Keep 16-bit sources at full precision; stbi_load would truncate them to 8 bits.
    const char* name = file.c_str();
    const PixelType type = stbi_is_hdr(name) ? PixelType::F32 : stbi_is_16_bit(name) ? PixelType::U16 : PixelType::U8;

    int   width = 0, height = 0, sourceChannels = 0;
    void* data = nullptr;
    switch (type)
    {
    case PixelType::U8:  data = stbi_load(name, &width, &height, &sourceChannels, kChannels);    break;
    case PixelType::U16: data = stbi_load_16(name, &width, &height, &sourceChannels, kChannels); break;
    case PixelType::F32: data = stbi_loadf(name, &width, &height, &sourceChannels, kChannels);   break;
    }

    if (!data)
    {
        return std::unexpected(stbi_failure_reason());
    }

    SourceImage image;
    image.width = uint32_t(width);
    image.height = uint32_t(height);
    image.type = type;
    image.pixels = PixelBuffer(data, &stbi_image_free);
    return image;
}

std::expected<SourceImage, std::string> LoadSourceImage(const std::filesystem::path& path)
{
    const std::string file = path.string();
    return IsEXR(file.c_str()) == TINYEXR_SUCCESS ? LoadExr(file) : LoadStb(file);
}