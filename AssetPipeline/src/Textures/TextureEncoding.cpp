#include <pch.h>
#include "TextureEncoding.h"

constexpr size_t EncodedSize(TextureFormat format, uint32_t width, uint32_t height)
{
    const FormatInfo& info = GetFormatInfo(format);
    if (info.blockWidth == 0) return 0;

    const size_t blocksX = (width + info.blockWidth - 1) / info.blockWidth;
    const size_t blocksY = (height + info.blockHeight - 1) / info.blockHeight;
    return blocksX * blocksY * info.bytesPerBlock;
}

EncodeResult EncodeTexture(const ImageView& image, const EncodeParams& params, std::span<std::byte> out)
{
    if (out.size() != EncodedSize(params.format, image.width, image.height))
        return EncodeResult(std::unexpect, "output buffer has the wrong size");

    switch (params.format)
    {
    case TextureFormat::BC7:
    case TextureFormat::BC7_SRGB:
        return EncodeWithBc7e(image, params, out);

    case TextureFormat::BC1:
    case TextureFormat::BC1_SRGB:
    case TextureFormat::BC4:
    case TextureFormat::BC5:
        return EncodeWithRgbcx(image, params, out);

    case TextureFormat::BC6H:
        return EncodeWithIspcTexcomp(image, params, out);

    case TextureFormat::RGBA8:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::RGBA16F:
    case TextureFormat::ETC2_RGB:
    case TextureFormat::ETC2_RGB_SRGB:
    case TextureFormat::ETC2_RGBA:
    case TextureFormat::ETC2_RGBA_SRGB:
    case TextureFormat::EAC_R11:
    case TextureFormat::EAC_RG11:
    case TextureFormat::ASTC_4x4:
    case TextureFormat::ASTC_4x4_SRGB:
    case TextureFormat::ASTC_6x6:
    case TextureFormat::ASTC_6x6_SRGB:
    case TextureFormat::ASTC_8x8:
    case TextureFormat::ASTC_8x8_SRGB:
    case TextureFormat::ASTC_4x4_HDR:
    case TextureFormat::ASTC_6x6_HDR:
        return EncodeResult(std::unexpect, std::format("{} is not implemented yet", GetFormatInfo(params.format).name));

    case TextureFormat::Unknown:
    case TextureFormat::Count:
        break;
    }

    return EncodeResult(std::unexpect, "invalid texture format");
}

void GatherBlockRGBA8(const ImageView& image, uint32_t bx, uint32_t by, uint32_t blockW, uint32_t blockH, std::span<std::byte> out)
{
    const uint32_t x0 = bx * blockW;
    const bool     fullRows = x0 + blockW <= image.width;

    for (uint32_t y = 0; y < blockH; ++y)
    {
        // Rows and columns past the image edge repeat the last pixel.
        const uint32_t  py = std::min(by * blockH + y, image.height - 1);
        const std::byte* src = image.pixels.data() + size_t(py) * image.width * 4;
        std::byte* dst = out.data() + size_t(y) * blockW * 4;

        if (fullRows)
        {
            std::memcpy(dst, src + size_t(x0) * 4, size_t(blockW) * 4);
            continue;
        }

        for (uint32_t x = 0; x < blockW; ++x)
        {
            const uint32_t px = std::min(x0 + x, image.width - 1);
            std::memcpy(dst + size_t(x) * 4, src + size_t(px) * 4, 4);
        }
    }
}