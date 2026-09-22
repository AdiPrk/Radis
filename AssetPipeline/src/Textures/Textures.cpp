#include <pch.h>
#include "Textures.h"
#include "ImageLoader.h"

static bool HasTransparency(const LinearImage& image)
{
    for (size_t i = 3; i < image.pixels.size(); i += 4)
    {
        if (image.pixels[i] < 1.0f) return true;
    }
    return false;
}

std::expected<CookedTexture, std::string> CookTexture(const std::filesystem::path& path, const TextureCookSettings& settings)
{
    // TODO: .dds/.ktx2 passthrough
    auto source = LoadSourceImage(path);
    if (!source)
    {
        return std::unexpected(source.error());
    }

    // Only color and linear data keeps HDR range; float normals and masks go to 8-bit formats.
    const bool         hdr = source->type == PixelType::F32 && RoleKeepsHdr(settings.role);
    const EncodeParams params{ ChooseFormat(settings.role, hdr, settings.target), settings.role, settings.quality };
    const FormatInfo& info = GetFormatInfo(params.format);

    LinearImage base = ToLinearImage(*source, settings.role);
    source->pixels.reset();   // only the working copy is needed from here on

    CookedTexture texture{ .format = params.format };
    if (!info.alpha && HasTransparency(base))
    {
        texture.warnings.push_back(std::format("alpha discarded, {} has no alpha channel", info.name));
    }

    std::vector<std::byte> pixels;   // encoder input, reused for every level

    const auto encodeLevel = [&](const LinearImage& level, uint32_t index) -> std::expected<void, std::string>
    {
        // Only the top mip must be aligned; smaller levels are exempt.
        if (index == 0 && settings.requireAlignedTopMip && (level.width % info.blockWidth || level.height % info.blockHeight))
        {
            return std::unexpected(std::format("{}x{} isn't a multiple of the {}x{} block size, which the platform requires for {}",
                level.width, level.height, info.blockWidth, info.blockHeight, info.name));
        }

        ToEncoderPixels(level, settings.role, params.format, pixels);
        const ImageView view{ level.width, level.height, hdr, pixels };

        CookedMip mip{ level.width, level.height, std::vector<std::byte>(EncodedSize(params.format, level.width, level.height)) };
        if (auto result = EncodeTexture(view, params, mip.data); !result)
        {
            return result;
        }

        texture.mips.push_back(std::move(mip));
        return {};
    };

    if (auto result = ForEachMip(std::move(base), settings.role, settings.mips, encodeLevel); !result)
    {
        return std::unexpected(result.error());
    }

    return texture;
}