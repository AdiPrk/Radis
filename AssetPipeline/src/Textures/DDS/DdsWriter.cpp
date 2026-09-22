#include <pch.h>
#include "DdsWriter.h"
#include <dxgiformat.h>

// Names and values from the DDS file format specification.
static constexpr uint32_t MakeFourCC(char a, char b, char c, char d)
{
    return uint32_t(uint8_t(a)) | uint32_t(uint8_t(b)) << 8 | uint32_t(uint8_t(c)) << 16 | uint32_t(uint8_t(d)) << 24;
}

static constexpr uint32_t DDS_MAGIC = MakeFourCC('D', 'D', 'S', ' ');
static constexpr uint32_t DDS_FOURCC_DX10 = MakeFourCC('D', 'X', '1', '0');
static constexpr uint32_t DDSD_CAPS = 0x00000001;
static constexpr uint32_t DDSD_HEIGHT = 0x00000002;
static constexpr uint32_t DDSD_WIDTH = 0x00000004;
static constexpr uint32_t DDSD_PIXELFORMAT = 0x00001000;
static constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000;
static constexpr uint32_t DDSD_LINEARSIZE = 0x00080000;
static constexpr uint32_t DDPF_FOURCC = 0x00000004;
static constexpr uint32_t DDSCAPS_COMPLEX = 0x00000008;
static constexpr uint32_t DDSCAPS_TEXTURE = 0x00001000;
static constexpr uint32_t DDSCAPS_MIPMAP = 0x00400000;
static constexpr uint32_t DDS_DIMENSION_TEXTURE2D = 3;

struct DdsPixelFormat
{
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rMask, gMask, bMask, aMask;
};

struct DdsHeader
{
    uint32_t       size;
    uint32_t       flags;
    uint32_t       height;
    uint32_t       width;
    uint32_t       pitchOrLinearSize;
    uint32_t       depth;
    uint32_t       mipMapCount;
    uint32_t       reserved1[11];
    DdsPixelFormat pixelFormat;
    uint32_t       caps, caps2, caps3, caps4;
    uint32_t       reserved2;
};

struct DdsHeaderDx10
{
    DXGI_FORMAT dxgiFormat;
    uint32_t    resourceDimension;
    uint32_t    miscFlag;
    uint32_t    arraySize;
    uint32_t    miscFlags2;
};

static_assert(sizeof(DdsHeader) == 124 && sizeof(DdsHeaderDx10) == 20, "DDS header layout mismatch");

static DXGI_FORMAT ToDxgiFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8:      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::RGBA16F:    return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::BC1:        return DXGI_FORMAT_BC1_UNORM;
    case TextureFormat::BC1_SRGB:   return DXGI_FORMAT_BC1_UNORM_SRGB;
    case TextureFormat::BC4:        return DXGI_FORMAT_BC4_UNORM;
    case TextureFormat::BC5:        return DXGI_FORMAT_BC5_UNORM;
    case TextureFormat::BC6H:       return DXGI_FORMAT_BC6H_UF16;
    case TextureFormat::BC7:        return DXGI_FORMAT_BC7_UNORM;
    case TextureFormat::BC7_SRGB:   return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:                        return DXGI_FORMAT_UNKNOWN;   // ETC2/EAC/ASTC have no DXGI equivalent
    }
}

std::expected<void, std::string> WriteDds(const std::filesystem::path& path, const CookedTexture& texture)
{
    if (texture.mips.empty())
    {
        return std::unexpected("texture has no mips");
    }

    const DXGI_FORMAT dxgiFormat = ToDxgiFormat(texture.format);
    if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
    {
        return std::unexpected(std::format("{} can't be stored in a .dds", GetFormatInfo(texture.format).name));
    }

    const CookedMip& top = texture.mips.front();
    const uint32_t   mipCount = uint32_t(texture.mips.size());

    DdsHeader header{};
    header.size = sizeof(DdsHeader);
    header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE | DDSD_MIPMAPCOUNT;
    header.height = top.height;
    header.width = top.width;
    header.pitchOrLinearSize = uint32_t(top.data.size());
    header.mipMapCount = mipCount;
    header.pixelFormat.size = sizeof(DdsPixelFormat);
    header.pixelFormat.flags = DDPF_FOURCC;
    header.pixelFormat.fourCC = DDS_FOURCC_DX10;
    header.caps = DDSCAPS_TEXTURE | (mipCount > 1 ? DDSCAPS_COMPLEX | DDSCAPS_MIPMAP : 0);

    const DdsHeaderDx10 dx10
    {
        .dxgiFormat = dxgiFormat,
        .resourceDimension = DDS_DIMENSION_TEXTURE2D,
        .miscFlag = 0,
        .arraySize = 1,
        .miscFlags2 = 0,
    };

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        return std::unexpected("cannot open " + path.string());
    }

    file.write(reinterpret_cast<const char*>(&DDS_MAGIC), sizeof(DDS_MAGIC));
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&dx10), sizeof(dx10));
    for (const CookedMip& mip : texture.mips)
    {
        file.write(reinterpret_cast<const char*>(mip.data.data()), std::streamsize(mip.data.size()));
    }

    if (!file)
    {
        return std::unexpected("failed writing " + path.string());
    }
    return {};
}