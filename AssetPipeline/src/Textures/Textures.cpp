#include <pch.h>
#include "Textures.h"
#include "ImageLoader.h"

std::expected<CookedTexture, std::string> CookTexture(const std::filesystem::path& path, const TextureCookSettings& settings)
{
    // TODO: .dds/.ktx2 passthrough, mips, max size
    auto source = LoadSourceImage(path);
    if (!source)
    {
        return std::unexpected(source.error());
    }

    if (source->hdr && !RoleKeepsHdr(settings.role))
    {
        ConvertToRgba8(*source, settings.role);
    }

    const ImageView    image = source->View();
    const EncodeParams params{ ChooseFormat(settings.role, image.hdr, settings.target), settings.role, settings.quality };

    CookedTexture texture;
    texture.format = params.format;
    texture.width = image.width;
    texture.height = image.height;
    texture.data.resize(EncodedSize(params.format, image.width, image.height));

    if (auto result = EncodeTexture(image, params, texture.data); !result)
    {
        return std::unexpected(result.error());
    }

    return texture;
}