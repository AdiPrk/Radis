#include <pch.h>
#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

static constexpr int kChannels = 4;

static void FreeMalloced(void* p) { std::free(p); }

ImageView SourceImage::View() const
{
    const size_t bytesPerPixel = hdr ? kChannels * sizeof(float) : kChannels;
    return { width, height, hdr, std::span(static_cast<const std::byte*>(pixels.get()), size_t(width) * height * bytesPerPixel) };
}

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
    image.hdr = true;
    image.pixels = PixelBuffer(rgba, &FreeMalloced); // tinyexr allocates with malloc
    return image;
}

static std::expected<SourceImage, std::string> LoadStb(const std::string& file)
{
    int width = 0, height = 0, sourceChannels = 0;
    const bool hdr = stbi_is_hdr(file.c_str()) != 0; 

    void* data = hdr 
        ? static_cast<void*>(stbi_loadf(file.c_str(), &width, &height, &sourceChannels, kChannels))
        : static_cast<void*>(stbi_load(file.c_str(), &width, &height, &sourceChannels, kChannels));

    if (!data)
    {
        return std::unexpected(stbi_failure_reason());
    }

    SourceImage image;
    image.width = uint32_t(width);
    image.height = uint32_t(height);
    image.hdr = hdr;
    image.pixels = PixelBuffer(data, &stbi_image_free);
    return image;
}

std::expected<SourceImage, std::string> LoadSourceImage(const std::filesystem::path& path)
{
    const std::string file = path.string();
    return IsEXR(file.c_str()) == TINYEXR_SUCCESS ? LoadExr(file) : LoadStb(file);
}

// todo, figure out how to deal with heightmap etc, eg using rgba16f instead of clamping to 0-1
void ConvertToRgba8(SourceImage& image, TextureRole role)
{
    if (!image.hdr)
        return;

    const size_t pixelCount = size_t(image.width) * image.height;
    const float* src = static_cast<const float*>(image.pixels.get());
    auto* dst = static_cast<uint8_t*>(std::malloc(pixelCount * kChannels));

    const auto toUnorm8 = [](float v) { return uint8_t(std::clamp(std::isnan(v) ? 0.0f : v, 0.0f, 1.0f) * 255.0f + 0.5f); };

    if (role == TextureRole::Normal)
    {
        // Encoded maps average about 0.5 in X and Y; raw -1..1 vectors average about 0.
        double sum = 0.0;
        for (size_t i = 0; i < pixelCount; ++i)
            sum += double(src[i * 4 + 0]) + src[i * 4 + 1];
        const bool rawVectors = sum / double(pixelCount * 2) < 0.25;

        for (size_t i = 0; i < pixelCount; ++i)
        {
            const float* p = src + i * 4;
            float x = p[0], y = p[1], z = p[2];
            if (!rawVectors)
            {
                x = x * 2.0f - 1.0f;
                y = y * 2.0f - 1.0f;
                z = z * 2.0f - 1.0f;
            }

            const float length = std::sqrt(x * x + y * y + z * z);
            if (length > 1e-6f)   // also false for NaN
            {
                x /= length; y /= length; z /= length;
            }
            else
            {
                x = 0.0f; y = 0.0f; z = 1.0f;   // degenerate: use flat
            }

            dst[i * 4 + 0] = toUnorm8(x * 0.5f + 0.5f);
            dst[i * 4 + 1] = toUnorm8(y * 0.5f + 0.5f);
            dst[i * 4 + 2] = toUnorm8(z * 0.5f + 0.5f);
            dst[i * 4 + 3] = toUnorm8(p[3]);
        }
    }
    else
    {
        for (size_t i = 0; i < pixelCount * kChannels; ++i)
            dst[i] = toUnorm8(src[i]);
    }

    image.pixels = PixelBuffer(dst, &FreeMalloced);
    image.hdr = false;
}