#include <pch.h>
#include "Textures.h"
#include "ImageLoader.h"

std::expected<CookedTexture, std::string> CookTexture(const std::filesystem::path& path, const TextureCookSettings& settings)
{
    // TODO: .dds/.ktx2 passthrough
    auto source = LoadSourceImage(path);
    if (!source)
    {
        return std::unexpected(source.error());
    }

    // Only color and linear data keeps HDR range; float normals and masks go to 8-bit formats.
    const bool         hdr = source->hdr && RoleKeepsHdr(settings.role);
    const EncodeParams params{ ChooseFormat(settings.role, hdr, settings.target), settings.role, settings.quality };

    LinearImage base = ToLinearImage(*source, settings.role);
    source->pixels.reset();   // only the working copy is needed from here on

    CookedTexture          texture{ .format = params.format };
    std::vector<std::byte> pixels;   // encoder input, reused for every level

    const auto encodeLevel = [&](const LinearImage& level, uint32_t) -> std::expected<void, std::string>
        {
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