#include <pch.h>
#include "Textures.h"
#include "Dilate.h"

struct AssembledImage
{
    LinearImage image;
    bool        floatSource = false;   // at least one input was HDR (.hdr, .exr)
};

static std::expected<void, std::string> ValidateChannels(const TextureRequest& request)
{
    std::vector<bool> used(request.inputs.size());
    for (const ChannelSource& source : request.channels)
    {
        if (source.input == ChannelSource::kConstant)
            continue;

        if (source.input < 0 || size_t(source.input) >= request.inputs.size() || source.channel > 3)
        {
            return std::unexpected(std::format("channel map refers to input {} channel {}, but there are {} inputs",
                source.input, source.channel, request.inputs.size()));
        }
        used[size_t(source.input)] = true;
    }

    if (const auto unused = std::ranges::find(used, false); unused != used.end())
    {
        return std::unexpected(std::format("input {} isn't used by the channel map", unused - used.begin()));
    }
    return {};
}

// Builds the working image from the request's inputs. Inputs are loaded one at a time, so only
// one decoded source is in memory at once, however many inputs a request has.
static std::expected<AssembledImage, std::string> AssembleImage(const TextureRequest& request)
{
    if (request.inputs.empty())
    {
        return std::unexpected("no inputs");
    }

    if (auto valid = ValidateChannels(request); !valid)
    {
        return std::unexpected(valid.error());
    }

    // sRGB decoding depends on where a value ends up: color channels of a Color texture are
    // sRGB-encoded, alpha and all other data aren't. Float sources are linear already.
    const bool srgbColor = request.settings.role == TextureRole::Color;

    AssembledImage assembled;
    LinearImage& image = assembled.image;

    for (size_t i = 0; i < request.inputs.size(); ++i)
    {
        const auto source = LoadSourceImage(request.inputs[i]);
        if (!source)
        {
            return std::unexpected(request.inputs.size() > 1 ? std::format("input {}: {}", i, source.error()) : source.error());
        }

        if (i == 0)
        {
            image = { source->width, source->height, std::vector<float>(size_t(source->width) * source->height * 4) };
        }
        else if (source->width != image.width || source->height != image.height)
        {
            return std::unexpected(std::format("input {} is {}x{}, but input 0 is {}x{}", i, source->width, source->height, image.width, image.height));
        }

        assembled.floatSource |= source->type == PixelType::F32;
        for (uint32_t c = 0; c < 4; ++c)
        {
            const ChannelSource& channel = request.channels[c];
            if (channel.input == int8_t(i))
            {
                DecodeChannel(*source, channel.channel, srgbColor && c < 3, image, c);
            }
        }
    }

    for (uint32_t c = 0; c < 4; ++c)
    {
        if (request.channels[c].input == ChannelSource::kConstant)
        {
            FillChannel(image, c, request.channels[c].constant);
        }
    }
    return assembled;
}

static bool HasTransparency(const LinearImage& image)
{
    for (size_t i = 3; i < image.pixels.size(); i += 4)
    {
        if (image.pixels[i] < 1.0f) return true;
    }
    return false;
}

std::expected<CookedTexture, std::string> CookTexture(const TextureRequest& request, const TextureBuildOptions& options)
{
    // TODO: .dds/.ktx2 passthrough
    auto assembled = AssembleImage(request);
    if (!assembled)
    {
        return std::unexpected(assembled.error());
    }

    const TextureSettings& settings = request.settings;
    LinearImage& base = assembled->image;

    // Only color and linear data keeps HDR range; float normals and masks go to 8-bit formats.
    const bool         hdr = assembled->floatSource && RoleKeepsHdr(settings.role);
    const EncodeParams params{ ChooseFormat(settings.role, hdr, options.target), settings.role, options.quality };
    const FormatInfo& info = GetFormatInfo(params.format);

    if (settings.role == TextureRole::Normal)
    {
        DecodeNormals(base, assembled->floatSource);
    }

    CookedTexture texture{ .format = params.format };
    if (HasTransparency(base))
    {
        if (!info.alpha)
        {
            texture.warnings.push_back(std::format("alpha discarded, {} has no alpha channel", info.name));
        }
        else if (RoleAlphaIsOpacity(settings.role))
        {
            DilateTransparentTexels(base);
        }
    }

    std::vector<std::byte> pixels;   // encoder input, reused for every level

    const auto encodeLevel = [&](const LinearImage& level, uint32_t index) -> std::expected<void, std::string>
        {
            // Only the top mip must be aligned; smaller levels are exempt.
            if (index == 0 && options.requireAlignedTopMip && (level.width % info.blockWidth || level.height % info.blockHeight))
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