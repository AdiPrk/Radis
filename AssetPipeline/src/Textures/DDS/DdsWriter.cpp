#include <pch.h>
#include "DdsWriter.h"
#include "DdsFormat.h"
#include "../../FileIO.h"

using namespace DdsFile;

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

    Header header{};
    header.size = sizeof(Header);
    header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT | (compressed ? DDSD_LINEARSIZE : DDSD_PITCH);
    header.height = top.height;
    header.width = top.width;
    header.pitchOrLinearSize = compressed ? uint32_t(top.data.size()) : top.width * info.bytesPerBlock;   // whole top level, or one row
    header.mipMapCount = mipCount;
    header.pixelFormat.size = sizeof(PixelFormat);
    header.pixelFormat.flags = DDPF_FOURCC;
    header.pixelFormat.fourCC = DDS_FOURCC_DX10;
    header.caps = DDSCAPS_TEXTURE | (mipCount > 1 ? DDSCAPS_COMPLEX | DDSCAPS_MIPMAP : 0);

    const HeaderDx10 dx10
    {
        .dxgiFormat = dxgiFormat,
        .resourceDimension = DDS_DIMENSION_TEXTURE2D,
        .miscFlag = 0,
        .arraySize = 1,
        .miscFlags2 = 0,
    };

    std::vector<std::span<const std::byte>> parts =
    {
        std::as_bytes(std::span(&DDS_MAGIC, 1)),
        std::as_bytes(std::span(&header, 1)),
        std::as_bytes(std::span(&dx10, 1)),
    };
    for (const CookedMip& mip : texture.mips)
    {
        parts.push_back(mip.data);
    }
    return WriteFileAtomic(path, parts);
}