#pragma once

#include "TextureFormats.h"
#include "TextureRoles.h"

struct ImageView
{
    uint32_t                   width = 0;
    uint32_t                   height = 0;
    bool                       hdr = false; // false: RGBA8, true: RGBA32F
    std::span<const std::byte> pixels;
};

enum class EncodeQuality : uint8_t { Fast, Normal, Best };

struct EncodeParams
{
    TextureFormat format  = TextureFormat::Unknown;
    TextureRole   role    = TextureRole::Color;
    EncodeQuality quality = EncodeQuality::Normal;
};

using EncodeResult = std::expected<void, std::string>;

constexpr size_t EncodedSize(TextureFormat format, uint32_t width, uint32_t height);

// Encodes one mip level into `out`, which must be EncodedSize(...) bytes.
EncodeResult EncodeTexture(const ImageView& image, const EncodeParams& params, std::span<std::byte> out);

// All the different libs
EncodeResult EncodeWithBc7e(const ImageView& image, const EncodeParams& params, std::span<std::byte> out);         // BC7
EncodeResult EncodeWithRgbcx(const ImageView& image, const EncodeParams& params, std::span<std::byte> out);        // BC1, BC4, BC5
EncodeResult EncodeWithIspcTexcomp(const ImageView& image, const EncodeParams& params, std::span<std::byte> out);  // BC6H

// Copies one block of RGBA8 pixels into `out`, repeating edge pixels past the border.
void GatherBlockRGBA8(const ImageView& image, uint32_t bx, uint32_t by, uint32_t blockW, uint32_t blockH, std::span<std::byte> out);