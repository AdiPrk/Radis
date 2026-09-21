// TextureFormats.h
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
};

constexpr FormatInfo kFormatInfo[] =
{
    // name             bW bH  bytes srgb   hdr
    { "Unknown",        0, 0,  0,    false, false },
    { "RGBA8",          1, 1,  4,    false, false },
    { "RGBA8_SRGB",     1, 1,  4,    true,  false },
    { "RGBA16F",        1, 1,  8,    false, true  },
    { "BC1",            4, 4,  8,    false, false },
    { "BC1_SRGB",       4, 4,  8,    true,  false },
    { "BC4",            4, 4,  8,    false, false },
    { "BC5",            4, 4, 16,    false, false },
    { "BC6H",           4, 4, 16,    false, true  },
    { "BC7",            4, 4, 16,    false, false },
    { "BC7_SRGB",       4, 4, 16,    true,  false },
    { "ETC2_RGB",       4, 4,  8,    false, false },
    { "ETC2_RGB_SRGB",  4, 4,  8,    true,  false },
    { "ETC2_RGBA",      4, 4, 16,    false, false },
    { "ETC2_RGBA_SRGB", 4, 4, 16,    true,  false },
    { "EAC_R11",        4, 4,  8,    false, false },
    { "EAC_RG11",       4, 4, 16,    false, false },
    { "ASTC_4x4",       4, 4, 16,    false, false },
    { "ASTC_4x4_SRGB",  4, 4, 16,    true,  false },
    { "ASTC_6x6",       6, 6, 16,    false, false },
    { "ASTC_6x6_SRGB",  6, 6, 16,    true,  false },
    { "ASTC_8x8",       8, 8, 16,    false, false },
    { "ASTC_8x8_SRGB",  8, 8, 16,    true,  false },
    { "ASTC_4x4_HDR",   4, 4, 16,    false, true  },
    { "ASTC_6x6_HDR",   6, 6, 16,    false, true  },
};
static_assert(std::size(kFormatInfo) == size_t(TextureFormat::Count), "kFormatInfo out of sync with TextureFormat");

constexpr const FormatInfo& GetFormatInfo(TextureFormat f) { return kFormatInfo[size_t(f)]; }