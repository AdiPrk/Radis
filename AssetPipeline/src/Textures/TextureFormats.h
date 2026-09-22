#pragma once

enum class TextureFormat : uint8_t
{
    Unknown,

    // uncompressed fallback
    RGBA8,
    RGBA8_SRGB,
    RGBA16F,

    // desktop
    BC1,
    BC1_SRGB,
    BC3,
    BC3_SRGB,
    BC4,
    BC5,
    BC6H,
    BC7,
    BC7_SRGB,

    // older mobile
    ETC2_RGB,
    ETC2_RGB_SRGB,
    ETC2_RGBA,
    ETC2_RGBA_SRGB,
    EAC_R11,
    EAC_RG11,

    // modern mobile
    ASTC_4x4,
    ASTC_4x4_SRGB,
    ASTC_6x6,
    ASTC_6x6_SRGB,
    ASTC_8x8,
    ASTC_8x8_SRGB,
    ASTC_4x4_HDR,
    ASTC_6x6_HDR,

    Count
};

struct FormatInfo
{
    const char* name;
    uint8_t     blockWidth;
    uint8_t     blockHeight;
    uint8_t     bytesPerBlock;
    bool        srgb;
    bool        hdr;
    bool        alpha;   // stores an alpha channel (BC1's 1-bit alpha isn't used)
};

constexpr FormatInfo kFormatInfo[] =
{
    // name             bW bH  bytes srgb   hdr    alpha
    { "Unknown",        0, 0,  0,    false, false, false },
    { "RGBA8",          1, 1,  4,    false, false, true  },
    { "RGBA8_SRGB",     1, 1,  4,    true,  false, true  },
    { "RGBA16F",        1, 1,  8,    false, true,  true  },
    { "BC1",            4, 4,  8,    false, false, false },
    { "BC1_SRGB",       4, 4,  8,    true,  false, false },
    { "BC3",            4, 4, 16,    false, false, true  },
    { "BC3_SRGB",       4, 4, 16,    true,  false, true  },
    { "BC4",            4, 4,  8,    false, false, false },
    { "BC5",            4, 4, 16,    false, false, false },
    { "BC6H",           4, 4, 16,    false, true,  false },
    { "BC7",            4, 4, 16,    false, false, true  },
    { "BC7_SRGB",       4, 4, 16,    true,  false, true  },
    { "ETC2_RGB",       4, 4,  8,    false, false, false },
    { "ETC2_RGB_SRGB",  4, 4,  8,    true,  false, false },
    { "ETC2_RGBA",      4, 4, 16,    false, false, true  },
    { "ETC2_RGBA_SRGB", 4, 4, 16,    true,  false, true  },
    { "EAC_R11",        4, 4,  8,    false, false, false },
    { "EAC_RG11",       4, 4, 16,    false, false, false },
    { "ASTC_4x4",       4, 4, 16,    false, false, true  },
    { "ASTC_4x4_SRGB",  4, 4, 16,    true,  false, true  },
    { "ASTC_6x6",       6, 6, 16,    false, false, true  },
    { "ASTC_6x6_SRGB",  6, 6, 16,    true,  false, true  },
    { "ASTC_8x8",       8, 8, 16,    false, false, true  },
    { "ASTC_8x8_SRGB",  8, 8, 16,    true,  false, true  },
    { "ASTC_4x4_HDR",   4, 4, 16,    false, true,  true  },
    { "ASTC_6x6_HDR",   6, 6, 16,    false, true,  true  },
};
static_assert(std::size(kFormatInfo) == size_t(TextureFormat::Count), "kFormatInfo out of sync with TextureFormat");

constexpr const FormatInfo& GetFormatInfo(TextureFormat f) { return kFormatInfo[size_t(f)]; }

// Bytes needed for one level of `width` x `height`, including partial blocks at the edges.
constexpr size_t EncodedSize(TextureFormat format, uint32_t width, uint32_t height)
{
    const FormatInfo& info = GetFormatInfo(format);
    if (info.blockWidth == 0) return 0;

    const size_t blocksX = (width + info.blockWidth - 1) / info.blockWidth;
    const size_t blocksY = (height + info.blockHeight - 1) / info.blockHeight;
    return blocksX * blocksY * info.bytesPerBlock;
}

// The same format without the sRGB label; both store identical blocks.
constexpr TextureFormat WithoutSrgb(TextureFormat f)
{
    switch (f)
    {
    case TextureFormat::RGBA8_SRGB:     return TextureFormat::RGBA8;
    case TextureFormat::BC1_SRGB:       return TextureFormat::BC1;
    case TextureFormat::BC3_SRGB:       return TextureFormat::BC3;
    case TextureFormat::BC7_SRGB:       return TextureFormat::BC7;
    case TextureFormat::ETC2_RGB_SRGB:  return TextureFormat::ETC2_RGB;
    case TextureFormat::ETC2_RGBA_SRGB: return TextureFormat::ETC2_RGBA;
    case TextureFormat::ASTC_4x4_SRGB:  return TextureFormat::ASTC_4x4;
    case TextureFormat::ASTC_6x6_SRGB:  return TextureFormat::ASTC_6x6;
    case TextureFormat::ASTC_8x8_SRGB:  return TextureFormat::ASTC_8x8;
    default:                            return f;
    }
}