/*****************************************************************//**
 * \file   DdsLoader.cpp
 * \brief  Loads DDS textures cooked by the asset pipeline.
 *********************************************************************/

#include <PCH/pch.h>
#include "DdsLoader.h"
#include "DdsFormat.h"

namespace Radis
{
    namespace
    {
        using namespace DdsFile;

        struct VulkanFormat
        {
            VkFormat format;
            uint32_t blockSize;       // texels per block side: 4 for BC formats, 1 otherwise
            uint32_t bytesPerBlock;
        };

        std::optional<VulkanFormat> ToVulkanFormat(DxgiFormat format)
        {
            switch (format)
            {
            case DxgiFormat::R8G8B8A8_UNorm:      return VulkanFormat{ VK_FORMAT_R8G8B8A8_UNORM, 1, 4 };
            case DxgiFormat::R8G8B8A8_UNorm_SRGB: return VulkanFormat{ VK_FORMAT_R8G8B8A8_SRGB, 1, 4 };
            case DxgiFormat::R16G16B16A16_Float:  return VulkanFormat{ VK_FORMAT_R16G16B16A16_SFLOAT, 1, 8 };
            case DxgiFormat::BC1_UNorm:           return VulkanFormat{ VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 4, 8 };
            case DxgiFormat::BC1_UNorm_SRGB:      return VulkanFormat{ VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 4, 8 };
            case DxgiFormat::BC3_UNorm:           return VulkanFormat{ VK_FORMAT_BC3_UNORM_BLOCK, 4, 16 };
            case DxgiFormat::BC3_UNorm_SRGB:      return VulkanFormat{ VK_FORMAT_BC3_SRGB_BLOCK, 4, 16 };
            case DxgiFormat::BC4_UNorm:           return VulkanFormat{ VK_FORMAT_BC4_UNORM_BLOCK, 4, 8 };
            case DxgiFormat::BC5_UNorm:           return VulkanFormat{ VK_FORMAT_BC5_UNORM_BLOCK, 4, 16 };
            case DxgiFormat::BC6H_UF16:           return VulkanFormat{ VK_FORMAT_BC6H_UFLOAT_BLOCK, 4, 16 };
            case DxgiFormat::BC7_UNorm:           return VulkanFormat{ VK_FORMAT_BC7_UNORM_BLOCK, 4, 16 };
            case DxgiFormat::BC7_UNorm_SRGB:      return VulkanFormat{ VK_FORMAT_BC7_SRGB_BLOCK, 4, 16 };
            default:                              return std::nullopt;
            }
        }

        // What the headers describe: the format, and where each mip lies within the image data.
        struct DdsImage
        {
            VkFormat                               format = VK_FORMAT_UNDEFINED;
            uint32_t                               width = 0;
            uint32_t                               height = 0;
            std::vector<TextureData::MipLevelInfo> mips;
            size_t                                 dataSize = 0;
        };

        // Reads the kDataOffset bytes of headers. Anything the asset pipeline doesn't write is
        // rejected with a logged error.
        std::optional<DdsImage> ReadHeaders(const unsigned char* bytes, const std::string& name)
        {
            uint32_t   magic = 0;
            Header     header{};
            HeaderDx10 dx10{};
            std::memcpy(&magic, bytes, sizeof(magic));
            std::memcpy(&header, bytes + sizeof(magic), sizeof(header));
            std::memcpy(&dx10, bytes + sizeof(magic) + sizeof(header), sizeof(dx10));

            if (magic != DDS_MAGIC || header.size != sizeof(Header) || header.pixelFormat.size != sizeof(PixelFormat))
            {
                RADIS_ERROR("{}: not a DDS file", name);
                return std::nullopt;
            }

            if (!(header.pixelFormat.flags & DDPF_FOURCC) || header.pixelFormat.fourCC != DDS_FOURCC_DX10)
            {
                RADIS_ERROR("{}: DDS files without a DX10 header aren't supported; cook the texture with the asset pipeline", name);
                return std::nullopt;
            }

            if (dx10.resourceDimension != DDS_DIMENSION_TEXTURE2D || dx10.arraySize != 1 || (dx10.miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE))
            {
                RADIS_ERROR("{}: only single 2D textures are supported (no arrays, cubemaps or volumes)", name);
                return std::nullopt;
            }

            const std::optional<VulkanFormat> format = ToVulkanFormat(dx10.dxgiFormat);
            if (!format)
            {
                RADIS_ERROR("{}: unsupported DXGI format {}", name, static_cast<uint32_t>(dx10.dxgiFormat));
                return std::nullopt;
            }

            if (header.width == 0 || header.height == 0)
            {
                RADIS_ERROR("{}: the image is empty", name);
                return std::nullopt;
            }

            const uint32_t fullChain = static_cast<uint32_t>(std::bit_width(std::max(header.width, header.height)));
            const uint32_t mipLevels = (header.flags & DDSD_MIPMAPCOUNT) ? std::max(header.mipMapCount, 1u) : 1u;
            if (mipLevels > fullChain)
            {
                RADIS_ERROR("{}: {} mips, but a {}x{} image has at most {}", name, mipLevels, header.width, header.height, fullChain);
                return std::nullopt;
            }

            DdsImage image;
            image.format = format->format;
            image.width = header.width;
            image.height = header.height;
            image.mips.reserve(mipLevels);
            for (uint32_t level = 0; level < mipLevels; ++level)
            {
                TextureData::MipLevelInfo mip{};
                mip.width = std::max(1u, header.width >> level);
                mip.height = std::max(1u, header.height >> level);

                const size_t blocksX = (mip.width + format->blockSize - 1) / format->blockSize;
                const size_t blocksY = (mip.height + format->blockSize - 1) / format->blockSize;
                mip.offset = image.dataSize;
                mip.size = blocksX * blocksY * format->bytesPerBlock;

                image.dataSize += mip.size;
                image.mips.push_back(mip);
            }
            return image;
        }

        // Fills the same fields the KTX2 path does; `outTexture.pixels` already holds the image data.
        void FillTexture(DdsImage&& image, const std::string& name, TextureData& outTexture)
        {
            outTexture.width = static_cast<int>(image.width);
            outTexture.height = static_cast<int>(image.height);
            outTexture.channels = 4;
            outTexture.name = name;
            outTexture.isCompressed = true;
            outTexture.imageFormat = image.format;
            outTexture.mipLevels = static_cast<uint32_t>(image.mips.size());
            outTexture.mipInfos = std::move(image.mips);
        }
    }

    bool DdsLoader::IsDds(const unsigned char* data, size_t size)
    {
        return data && size >= sizeof(DDS_MAGIC) && std::memcmp(data, &DDS_MAGIC, sizeof(DDS_MAGIC)) == 0;
    }

    bool DdsLoader::FromMemory(const unsigned char* data, size_t size, const std::string& name, TextureData& outTexture)
    {
        if (!data || size < kDataOffset)
        {
            RADIS_ERROR("{}: too small to be a DDS file", name);
            return false;
        }

        std::optional<DdsImage> image = ReadHeaders(data, name);
        if (!image)
        {
            return false;
        }

        if (size - kDataOffset < image->dataSize)
        {
            RADIS_ERROR("{}: truncated, {} bytes of image data where {} are needed", name, size - kDataOffset, image->dataSize);
            return false;
        }

        outTexture.pixels.assign(data + kDataOffset, data + kDataOffset + image->dataSize);
        FillTexture(std::move(*image), name, outTexture);
        return true;
    }

    bool DdsLoader::FromFile(const std::string& path, TextureData& outTexture)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            RADIS_ERROR("{}: cannot open", path);
            return false;
        }

        // Headers first, then the image data straight into the texture: one read, no extra copy.
        const std::streamoff                     fileSize = file.tellg();
        std::array<unsigned char, kDataOffset> headers{};
        file.seekg(0);
        if (fileSize < std::streamoff(kDataOffset) || !file.read(reinterpret_cast<char*>(headers.data()), headers.size()))
        {
            RADIS_ERROR("{}: too small to be a DDS file", path);
            return false;
        }

        std::optional<DdsImage> image = ReadHeaders(headers.data(), path);
        if (!image)
        {
            return false;
        }

        const size_t available = static_cast<size_t>(fileSize) - kDataOffset;
        if (available < image->dataSize)
        {
            RADIS_ERROR("{}: truncated, {} bytes of image data where {} are needed", path, available, image->dataSize);
            return false;
        }

        outTexture.pixels.resize(image->dataSize);
        if (!file.read(reinterpret_cast<char*>(outTexture.pixels.data()), static_cast<std::streamsize>(image->dataSize)))
        {
            RADIS_ERROR("{}: failed reading the image data", path);
            return false;
        }

        FillTexture(std::move(*image), path, outTexture);
        return true;
    }
}