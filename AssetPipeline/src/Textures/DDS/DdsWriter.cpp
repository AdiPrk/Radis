#include <pch.h>
#include "DdsWriter.h"

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
static constexpr uint32_t DDSD_PITCH = 0x00000008;
static constexpr uint32_t DDSD_PIXELFORMAT = 0x00001000;
static constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000;
static constexpr uint32_t DDSD_LINEARSIZE = 0x00080000;
static constexpr uint32_t DDPF_FOURCC = 0x00000004;
static constexpr uint32_t DDSCAPS_COMPLEX = 0x00000008;
static constexpr uint32_t DDSCAPS_TEXTURE = 0x00001000;
static constexpr uint32_t DDSCAPS_MIPMAP = 0x00400000;
static constexpr uint32_t DDS_DIMENSION_TEXTURE2D = 3;

// The DXGI_FORMAT values the writer needs, so the tool doesn't depend on the Windows SDK.
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
    DxgiFormat  dxgiFormat;
    uint32_t    resourceDimension;
    uint32_t    miscFlag;
    uint32_t    arraySize;
    uint32_t    miscFlags2;
};

static_assert(sizeof(DdsHeader) == 124 && sizeof(DdsHeaderDx10) == 20, "DDS header layout mismatch");

static DxgiFormat ToDxgiFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8:      return DxgiFormat::R8G8B8A8_UNorm;
    case TextureFormat::RGBA8_SRGB: return DxgiFormat::R8G8B8A8_UNorm_SRGB;
    case TextureFormat::RGBA16F:    return DxgiFormat::R16G16B16A16_Float;
    case TextureFormat::BC1:        return DxgiFormat::BC1_UNorm;
    case TextureFormat::BC1_SRGB:   return DxgiFormat::BC1_UNorm_SRGB;
    case TextureFormat::BC3:        return DxgiFormat::BC3_UNorm;
    case TextureFormat::BC3_SRGB:   return DxgiFormat::BC3_UNorm_SRGB;
    case TextureFormat::BC4:        return DxgiFormat::BC4_UNorm;
    case TextureFormat::BC5:        return DxgiFormat::BC5_UNorm;
    case TextureFormat::BC6H:       return DxgiFormat::BC6H_UF16;
    case TextureFormat::BC7:        return DxgiFormat::BC7_UNorm;
    case TextureFormat::BC7_SRGB:   return DxgiFormat::BC7_UNorm_SRGB;
    default:                        return DxgiFormat::Unknown;   // ETC2/EAC/ASTC have no DXGI equivalent
    }
}

std::expected<void, std::string> WriteDds(const std::filesystem::path& path, const CookedTexture& texture)
{
    if (texture.mips.empty())
    {
        return std::unexpected("texture has no mips");
    }

    const DxgiFormat dxgiFormat = ToDxgiFormat(texture.format);
    if (dxgiFormat == DxgiFormat::Unknown)
    {
        return std::unexpected(std::format("{} can't be stored in a .dds", GetFormatInfo(texture.format).name));
    }

    const FormatInfo& info = GetFormatInfo(texture.format);
    const CookedMip& top = texture.mips.front();
    const uint32_t    mipCount = uint32_t(texture.mips.size());
    const bool        compressed = info.blockWidth > 1;

    DdsHeader header{};
    header.size = sizeof(DdsHeader);
    header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT | (compressed ? DDSD_LINEARSIZE : DDSD_PITCH);
    header.height = top.height;
    header.width = top.width;
    header.pitchOrLinearSize = compressed ? uint32_t(top.data.size()) : top.width * info.bytesPerBlock;   // whole top level, or one row
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

    // Write next to the target and rename, so a failed or interrupted write never leaves a truncated file behind.
    std::filesystem::path temp = path;
    temp += ".tmp";

    std::ofstream file(temp, std::ios::binary);
    if (!file)
    {
        return std::unexpected("cannot open " + temp.string());
    }

    file.write(reinterpret_cast<const char*>(&DDS_MAGIC), sizeof(DDS_MAGIC));
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&dx10), sizeof(dx10));
    for (const CookedMip& mip : texture.mips)
    {
        file.write(reinterpret_cast<const char*>(mip.data.data()), std::streamsize(mip.data.size()));
    }
    file.close();

    if (!file)
    {
        std::filesystem::remove(temp, ec);
        return std::unexpected("failed writing " + temp.string());
    }

    std::filesystem::rename(temp, path, ec);   // replaces an existing file
    if (ec)
    {
        const std::string reason = ec.message();
        std::filesystem::remove(temp, ec);
        return std::unexpected(std::format("cannot replace {}: {}", path.string(), reason));
    }
    return {};
}