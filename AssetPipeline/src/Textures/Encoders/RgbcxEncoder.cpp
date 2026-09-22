#include <pch.h>
#include "../TextureEncoding.h"

#include <bc7enc_rdo/rgbcx.h>

// rgbcx levels run 0..18: higher is slower and slightly better. 10 is near the top of the
// quality curve at a fraction of level 18's cost.
static uint32_t LevelFor(EncodeQuality quality)
{
    switch (quality)
    {
    case EncodeQuality::Fast:   return 4;
    case EncodeQuality::Normal: return 10;
    case EncodeQuality::Best:   return rgbcx::MAX_LEVEL;
    }
    return 10;
}

// One block of 16 RGBA8 pixels -> one encoded block. The "hq" variants search more alpha and
// single-channel endpoints, which matters most for normal maps and masks.
using BlockEncoder = void (*)(uint32_t level, bool hq, const uint8_t* pixels, void* dst);

static BlockEncoder EncoderFor(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::BC1:
    case TextureFormat::BC1_SRGB:
        return [](uint32_t level, bool, const uint8_t* pixels, void* dst)
            {
                // Alpha is ignored. 3-color mode improves quality, but its fourth texel is black,
                // so it's only safe when the texture has no alpha; color roles with alpha use BC7.
                rgbcx::encode_bc1(level, dst, pixels, true, false);
            };

    case TextureFormat::BC3:
    case TextureFormat::BC3_SRGB:
        return [](uint32_t level, bool hq, const uint8_t* pixels, void* dst)
            {
                if (hq)
                {
                    rgbcx::encode_bc3_hq(level, dst, pixels);
                }
                else
                {
                    rgbcx::encode_bc3(level, dst, pixels);
                }
            };

    case TextureFormat::BC4:
        return [](uint32_t, bool hq, const uint8_t* pixels, void* dst)
            {
                if (hq)   // red channel only
                {
                    rgbcx::encode_bc4_hq(dst, pixels);
                }
                else
                {
                    rgbcx::encode_bc4(dst, pixels);
                }
            };

    case TextureFormat::BC5:
        return [](uint32_t, bool hq, const uint8_t* pixels, void* dst)
            {
                if (hq)   // red and green channels
                {
                    rgbcx::encode_bc5_hq(dst, pixels);
                }
                else
                {
                    rgbcx::encode_bc5(dst, pixels);
                }
            };

    default:
        return nullptr;
    }
}

EncodeResult EncodeWithRgbcx(const ImageView& image, const EncodeParams& params, std::span<std::byte> out)
{
    static const bool initialized = (rgbcx::init(), true);   // runs once, thread-safe
    (void)initialized;

    if (image.hdr)
    {
        return std::unexpected("rgbcx needs RGBA8 input");
    }

    const FormatInfo& info = GetFormatInfo(params.format);
    const BlockEncoder encodeBlock = EncoderFor(params.format);
    if (!encodeBlock)
    {
        return std::unexpected(std::format("{} is not an rgbcx format", info.name));
    }

    const uint32_t level = LevelFor(params.quality);
    const bool     hq = params.quality == EncodeQuality::Best;
    const uint32_t blocksX = (image.width + 3) / 4;
    const uint32_t blocksY = (image.height + 3) / 4;

    std::array<std::byte, 64> block{};   // 16 RGBA8 pixels
    for (uint32_t by = 0; by < blocksY; ++by)
    {
        for (uint32_t bx = 0; bx < blocksX; ++bx)
        {
            GatherBlockRGBA8(image, bx, by, 4, 4, block);
            encodeBlock(level, hq, reinterpret_cast<const uint8_t*>(block.data()),
                out.data() + (size_t(by) * blocksX + bx) * info.bytesPerBlock);
        }
    }
    return {};
}