#include <pch.h>
#include "../TextureEncoding.h"

#include <ispc_texcomp/ispc_texcomp.h>

// BC6H stores unsigned half floats, and ispc_texcomp wants the input in that form.
// Values are already clamped to [0, 65504] with NaN removed by ToEncoderPixels.
static uint16_t FloatToHalf(float value)
{
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    const uint32_t sign = (bits >> 16) & 0x8000;
    const int32_t  exponent = int32_t((bits >> 23) & 0xFF) - 127 + 15;
    const uint32_t mantissa = bits & 0x7FFFFF;

    if (exponent >= 31)
    {
        return uint16_t(sign | 0x7BFF);   // clamp to the largest half (65504)
    }

    if (exponent <= 0)
    {
        if (exponent < -10)
        {
            return uint16_t(sign);   // too small to represent
        }

        // Subnormal: shift the implicit leading 1 into place.
        const uint32_t shifted = (mantissa | 0x800000) >> uint32_t(14 - exponent);
        return uint16_t(sign | shifted);
    }

    uint16_t half = uint16_t(sign | (uint32_t(exponent) << 10) | (mantissa >> 13));
    if ((mantissa & 0x1FFF) > 0x1000)
    {
        ++half;   // round to nearest; a carry into the exponent is correct
    }
    return half;
}

EncodeResult EncodeWithIspcTexcomp(const ImageView& image, const EncodeParams& params, std::span<std::byte> out)
{
    if (params.format != TextureFormat::BC6H)
    {
        return std::unexpected(std::format("{} is not an ispc_texcomp format here", GetFormatInfo(params.format).name));
    }

    if (!image.hdr)
    {
        return std::unexpected("BC6H needs RGBA32F input");
    }

    bc6h_enc_settings settings{};
    switch (params.quality)
    {
    case EncodeQuality::Fast:   GetProfile_bc6h_veryfast(&settings); break;
    case EncodeQuality::Normal: GetProfile_bc6h_basic(&settings);    break;
    case EncodeQuality::Best:   GetProfile_bc6h_slow(&settings);     break;
    }

    // ispc_texcomp encodes whole blocks only, so the surface is padded out to a multiple of
    // 4 with repeated edge pixels, exactly like GatherBlockRGBA8 does for the other encoders.
    const uint32_t blocksX = (image.width + 3) / 4;
    const uint32_t blocksY = (image.height + 3) / 4;
    const uint32_t paddedWidth = blocksX * 4;
    const uint32_t paddedHeight = blocksY * 4;

    std::vector<uint16_t> halfPixels(size_t(paddedWidth) * paddedHeight * 4);
    const auto* source = reinterpret_cast<const float*>(image.pixels.data());

    for (uint32_t y = 0; y < paddedHeight; ++y)
    {
        const float* srcRow = source + size_t(std::min(y, image.height - 1)) * image.width * 4;
        uint16_t* dstRow = halfPixels.data() + size_t(y) * paddedWidth * 4;

        for (uint32_t x = 0; x < paddedWidth; ++x)
        {
            const float* src = srcRow + size_t(std::min(x, image.width - 1)) * 4;
            for (size_t c = 0; c < 4; ++c)
            {
                dstRow[size_t(x) * 4 + c] = FloatToHalf(src[c]);
            }
        }
    }

    rgba_surface surface{};
    surface.ptr = reinterpret_cast<uint8_t*>(halfPixels.data());
    surface.width = int32_t(paddedWidth);
    surface.height = int32_t(paddedHeight);
    surface.stride = int32_t(paddedWidth * 4 * sizeof(uint16_t));

    CompressBlocksBC6H(&surface, reinterpret_cast<uint8_t*>(out.data()), &settings);
    return {};
}