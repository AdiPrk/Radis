#pragma once

#include <cstdint>

// Layout of the DDS files the asset pipeline writes. Shared by the pipeline, which writes them, and
// the engine, which loads them, so it has no other dependencies. Names and values follow the DDS
// file format specification.
//
// The pipeline always writes: the magic, a Header, a HeaderDx10 (so the format is a DXGI_FORMAT),
// then every mip level of one 2D image, largest first, each tightly packed (rows of 4x4 blocks for
// block-compressed formats, rows of pixels otherwise).
namespace DdsFile
{
    constexpr uint32_t MakeFourCC(char a, char b, char c, char d)
    {
        return uint32_t(uint8_t(a)) | uint32_t(uint8_t(b)) << 8 | uint32_t(uint8_t(c)) << 16 | uint32_t(uint8_t(d)) << 24;
    }

    inline constexpr uint32_t DDS_MAGIC = MakeFourCC('D', 'D', 'S', ' ');
    inline constexpr uint32_t DDS_FOURCC_DX10 = MakeFourCC('D', 'X', '1', '0');

    // Header::flags
    inline constexpr uint32_t DDSD_CAPS = 0x00000001;
    inline constexpr uint32_t DDSD_HEIGHT = 0x00000002;
    inline constexpr uint32_t DDSD_WIDTH = 0x00000004;
    inline constexpr uint32_t DDSD_PITCH = 0x00000008;
    inline constexpr uint32_t DDSD_PIXELFORMAT = 0x00001000;
    inline constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000;
    inline constexpr uint32_t DDSD_LINEARSIZE = 0x00080000;

    // PixelFormat::flags
    inline constexpr uint32_t DDPF_FOURCC = 0x00000004;

    // Header::caps
    inline constexpr uint32_t DDSCAPS_COMPLEX = 0x00000008;
    inline constexpr uint32_t DDSCAPS_TEXTURE = 0x00001000;
    inline constexpr uint32_t DDSCAPS_MIPMAP = 0x00400000;

    // HeaderDx10
    inline constexpr uint32_t DDS_DIMENSION_TEXTURE2D = 3;
    inline constexpr uint32_t DDS_RESOURCE_MISC_TEXTURECUBE = 0x00000004;

    // The DXGI_FORMAT values the pipeline writes, so neither side depends on the Windows SDK.
    enum class DxgiFormat : uint32_t
    {
        Unknown = 0,
        R16G16B16A16_Float = 10,
        R8G8B8A8_UNorm = 28,
        R8G8B8A8_UNorm_SRGB = 29,
        BC1_UNorm = 71,
        BC1_UNorm_SRGB = 72,
        BC3_UNorm = 77,
        BC3_UNorm_SRGB = 78,
        BC4_UNorm = 80,
        BC5_UNorm = 83,
        BC6H_UF16 = 95,
        BC7_UNorm = 98,
        BC7_UNorm_SRGB = 99,
    };

    struct PixelFormat
    {
        uint32_t size;
        uint32_t flags;
        uint32_t fourCC;
        uint32_t rgbBitCount;
        uint32_t rMask, gMask, bMask, aMask;
    };

    struct Header
    {
        uint32_t    size;
        uint32_t    flags;
        uint32_t    height;
        uint32_t    width;
        uint32_t    pitchOrLinearSize;
        uint32_t    depth;
        uint32_t    mipMapCount;
        uint32_t    reserved1[11];
        PixelFormat pixelFormat;
        uint32_t    caps, caps2, caps3, caps4;
        uint32_t    reserved2;
    };

    struct HeaderDx10
    {
        DxgiFormat dxgiFormat;
        uint32_t   resourceDimension;
        uint32_t   miscFlag;
        uint32_t   arraySize;
        uint32_t   miscFlags2;
    };

    // Where the image data starts.
    inline constexpr uint32_t kDataOffset = sizeof(uint32_t) + sizeof(Header) + sizeof(HeaderDx10);

    static_assert(sizeof(PixelFormat) == 32 && sizeof(Header) == 124 && sizeof(HeaderDx10) == 20, "DDS header layout mismatch");
}