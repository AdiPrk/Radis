#include <pch.h>
#include "ImageLoader.h"
#include "../FileIO.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

static constexpr int kChannels = 4;

static void FreeMalloced(void* p) { std::free(p); }

static std::expected<SourceImage, std::string> LoadExr(std::span<const std::byte> encoded)
{
    float* rgba = nullptr;
    int         width = 0;
    int         height = 0;
    const char* error = nullptr;

    // Loads the default layer as RGBA float; alpha is 1 if the file has none.
    const auto* data = reinterpret_cast<const unsigned char*>(encoded.data());
    if (LoadEXRFromMemory(&rgba, &width, &height, data, encoded.size(), &error) != TINYEXR_SUCCESS)
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

static std::expected<SourceImage, std::string> LoadStb(std::span<const std::byte> encoded)
{
    if (encoded.size() > size_t(std::numeric_limits<int>::max()))
    {
        return std::unexpected("image file is too large");   // stb takes an int length
    }

    // Keep 16-bit sources at full precision; stbi_load would truncate them to 8 bits.
    const auto* data = reinterpret_cast<const stbi_uc*>(encoded.data());
    const int       size = int(encoded.size());
    const PixelType type = stbi_is_hdr_from_memory(data, size) ? PixelType::F32
        : stbi_is_16_bit_from_memory(data, size) ? PixelType::U16 : PixelType::U8;

    int   width = 0, height = 0, sourceChannels = 0;
    void* pixels = nullptr;
    switch (type)
    {
    case PixelType::U8:  pixels = stbi_load_from_memory(data, size, &width, &height, &sourceChannels, kChannels);    break;
    case PixelType::U16: pixels = stbi_load_16_from_memory(data, size, &width, &height, &sourceChannels, kChannels); break;
    case PixelType::F32: pixels = stbi_loadf_from_memory(data, size, &width, &height, &sourceChannels, kChannels);   break;
    }

    if (!pixels)
    {
        return std::unexpected(stbi_failure_reason());
    }

    SourceImage image;
    image.width = uint32_t(width);
    image.height = uint32_t(height);
    image.type = type;
    image.pixels = PixelBuffer(pixels, &stbi_image_free);
    return image;
}

std::expected<SourceImage, std::string> LoadSourceImage(std::span<const std::byte> encoded)
{
    const auto* data = reinterpret_cast<const unsigned char*>(encoded.data());
    return IsEXRFromMemory(data, encoded.size()) == TINYEXR_SUCCESS ? LoadExr(encoded) : LoadStb(encoded);
}

// The encoded image: the source's own bytes, or its file read into `storage`. One read, then
// decoding from memory, gives files and embedded images a single code path.
static std::expected<std::span<const std::byte>, std::string> EncodedBytes(const ImageSource& source, std::vector<std::byte>& storage)
{
    if (!source.bytes.empty())
    {
        return std::span(source.bytes);
    }

    auto bytes = ReadFile(source.path);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    storage = std::move(*bytes);
    return std::span(storage);
}

std::expected<SourceImage, std::string> LoadSourceImage(const ImageSource& source)
{
    std::vector<std::byte> storage;
    return EncodedBytes(source, storage).and_then([](std::span<const std::byte> encoded) { return LoadSourceImage(encoded); });
}

// False only when the header shows no alpha channel; anything unclear is left to decoding.
static bool MayHaveAlpha(std::span<const std::byte> encoded)
{
    const auto* data = reinterpret_cast<const stbi_uc*>(encoded.data());
    int width = 0, height = 0, channels = 0;
    if (IsEXRFromMemory(data, encoded.size()) == TINYEXR_SUCCESS || encoded.size() > size_t(std::numeric_limits<int>::max())
        || !stbi_info_from_memory(data, int(encoded.size()), &width, &height, &channels))
    {
        return true;
    }
    return channels == 2 || channels == 4;   // gray + alpha, RGBA
}

std::expected<AlphaUsage, std::string> MeasureAlpha(const ImageSource& source)
{
    std::vector<std::byte> storage;
    const auto encoded = EncodedBytes(source, storage);
    if (!encoded)
    {
        return std::unexpected(encoded.error());
    }
    if (!MayHaveAlpha(*encoded))
    {
        return AlphaUsage::Opaque;
    }

    const auto image = LoadSourceImage(*encoded);
    if (!image)
    {
        return std::unexpected(image.error());
    }

    size_t clear = 0, partial = 0;
    const auto count = [&](const auto* pixels, float scale)
        {
            const size_t pixelCount = size_t(image->width) * image->height;
            for (size_t i = 0; i < pixelCount; ++i)
            {
                const float alpha = float(pixels[i * kChannels + 3]) * scale;
                clear += alpha <= 0.02f;
                partial += alpha > 0.02f && alpha < 0.98f;
            }
        };
    switch (image->type)
    {
    case PixelType::U8:  count(static_cast<const uint8_t*>(image->pixels.get()), 1.0f / 255.0f);    break;
    case PixelType::U16: count(static_cast<const uint16_t*>(image->pixels.get()), 1.0f / 65535.0f); break;
    case PixelType::F32: count(static_cast<const float*>(image->pixels.get()), 1.0f);               break;
    }

    if (clear == 0 && partial == 0)
    {
        return AlphaUsage::Opaque;
    }
    return partial > clear ? AlphaUsage::Translucent : AlphaUsage::Cutout;
}