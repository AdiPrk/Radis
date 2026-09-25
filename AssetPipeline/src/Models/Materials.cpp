#include <pch.h>
#include "Materials.h"
#include "../FileIO.h"
#include "../OutputLayout.h"

using ModelFile::MaterialTexture;

static constexpr const char* kSlotNames[] = { "BaseColor", "Normal", "ORM", "Emissive", "Transmission" };
static_assert(std::size(kSlotNames) == ModelFile::kMaterialTextureCount);

// An image ready to become a texture input.
struct ImageRef
{
    std::string key;    // identity; see PathKey
    std::string stem;   // the file's name without extension; empty for embedded images
    ImageSource source;

    bool operator==(const ImageRef& other) const { return key == other.key; }
};

static std::optional<ImageRef> ResolveImage(const ImportedTexture& texture, MaterialContext& ctx)
{
    if (texture.embedded >= 0)
    {
        return ImageRef{ std::format("{}#embedded/{}", ctx.modelKey, texture.embedded), {}, {.bytes = ctx.scene.embeddedImages[size_t(texture.embedded)] } };
    }
    if (texture.path.empty())
    {
        return std::nullopt;
    }

    std::string key = PathKey(texture.path);
    ctx.usedImages.insert(key);

    const std::u8string stem = texture.path.stem().u8string();
    return ImageRef{ std::move(key), std::string(stem.begin(), stem.end()), {.path = texture.path } };
}

static std::optional<ImageRef> ResolveSlot(const ImportedMaterial& material, SourceSlot slot, MaterialContext& ctx)
{
    return ResolveImage(material.Texture(slot), ctx);
}

// Queues a texture and returns its file name. `recipe` completes the key: the same images cooked
// a different way are a different texture.
static std::expected<std::string, std::string> Queue(const std::vector<const ImageRef*>& images, std::string_view recipe, std::string name,
    const ChannelMap& channels, TextureRole role, MaterialContext& ctx)
{
    TextureRequest request{ .name = std::move(name), .folder = kModelTextureFolder, .channels = channels, .settings = {.role = role } };
    for (const ImageRef* image : images)
    {
        request.key += image->key + "|";
        request.inputs.push_back(image->source);
    }
    request.key += recipe;
    return ctx.textures.Add(std::move(request));
}

// A texture from one image file is named after it; embedded images and packed textures belong to
// the model, so they're named <model>_<material>_<slot>.
static std::string TextureName(const std::vector<const ImageRef*>& images, const ImportedMaterial& material, MaterialTexture slot, MaterialContext& ctx)
{
    if (images.size() == 1 && !images[0]->stem.empty())
    {
        return images[0]->stem;
    }
    return std::format("{}_{}_{}", ctx.modelName, material.name, kSlotNames[size_t(slot)]);
}

static constexpr ChannelSource kOne = ChannelSource::Constant(1.0f);

// RGB of one image; alpha is ignored and set to 1 so the encoder spends no bits on it.
static constexpr ChannelMap kOpaque = { { { 0, 0 }, { 0, 1 }, { 0, 2 }, kOne } };

static ModelFile::AlphaMode ToAlphaMode(AlphaUsage usage)
{
    switch (usage)
    {
    case AlphaUsage::Cutout:      return ModelFile::AlphaMode::Mask;
    case AlphaUsage::Translucent: return ModelFile::AlphaMode::Blend;
    default:                      return ModelFile::AlphaMode::Opaque;
    }
}

// Base color keeps alpha only when the material uses it. A separate opacity map goes into alpha.
// Sets `alphaMode`, which may come from the color image itself (see ImportedMaterial::alphaFromTexture).
static std::expected<std::string, std::string> BuildBaseColor(const ImportedMaterial& material, ModelFile::AlphaMode& alphaMode, MaterialContext& ctx)
{
    const auto color = ResolveSlot(material, SourceSlot::BaseColor, ctx);

    alphaMode = material.alphaMode;
    if (color && material.alphaFromTexture)
    {
        const auto usage = MeasureAlpha(color->source);
        alphaMode = usage ? ToAlphaMode(*usage) : ModelFile::AlphaMode::Opaque;   // an unreadable image fails when it's cooked
    }

    const auto opacity = alphaMode != ModelFile::AlphaMode::Opaque ? ResolveSlot(material, SourceSlot::Opacity, ctx) : std::nullopt;
    if (!opacity || opacity == color)   // an opacity map naming the color image means its own alpha
    {
        if (!color)
            return std::string();

        const std::vector images = { &*color };
        const std::string name = TextureName(images, material, MaterialTexture::BaseColor, ctx);
        return alphaMode == ModelFile::AlphaMode::Opaque
            ? Queue(images, "color-opaque", name, kOpaque, TextureRole::Color, ctx)
            : Queue(images, "color", name, kPassThrough, TextureRole::Color, ctx);
    }

    // Opacity from its own image's first channel; without a color image, color comes from the factor.
    std::vector<const ImageRef*> images;
    ChannelMap                   channels = { kOne, kOne, kOne, { 0, 0 } };
    if (color)
    {
        images.push_back(&*color);
        channels = { { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 0 } } };
    }
    images.push_back(&*opacity);

    const std::string name = TextureName(images, material, MaterialTexture::BaseColor, ctx);
    return Queue(images, color ? "color+opacity" : "opacity", name, channels, TextureRole::Color, ctx);
}

// Occlusion, roughness and metalness packed into R, G and B. A glTF file already stores roughness
// in G and metalness in B of one image (often with occlusion in its R), so it passes straight
// through; separate grayscale images are read from their first channel. Missing channels are 1,
// which leaves the material factor in charge.
static std::expected<std::string, std::string> BuildOrm(const ImportedMaterial& material, MaterialContext& ctx)
{
    const auto occlusion = ResolveSlot(material, SourceSlot::Occlusion, ctx);
    const auto roughness = ResolveSlot(material, SourceSlot::Roughness, ctx);
    const auto metalness = ResolveSlot(material, SourceSlot::Metalness, ctx);
    if (!occlusion && !roughness && !metalness)
    {
        return std::string();
    }

    std::vector<const ImageRef*> images;
    const auto inputFor = [&](const ImageRef& image) -> int8_t
        {
            const auto found = std::ranges::find_if(images, [&](const ImageRef* i) { return *i == image; });
            if (found != images.end())
            {
                return int8_t(found - images.begin());
            }
            images.push_back(&image);
            return int8_t(images.size() - 1);
        };

    const bool combined = roughness && roughness == metalness;
    ChannelMap channels = { kOne, kOne, kOne, kOne };
    if (occlusion) channels[0] = { inputFor(*occlusion), 0 };
    if (roughness) channels[1] = { inputFor(*roughness), uint8_t(combined ? 1 : 0) };
    if (metalness) channels[2] = { inputFor(*metalness), uint8_t(combined ? 2 : 0) };

    // Which image feeds which channel is part of the identity.
    const std::string recipe = std::format("orm{}{}{}{}{}{}", channels[0].input, channels[0].channel, channels[1].input, channels[1].channel,
        channels[2].input, channels[2].channel);
    return Queue(images, recipe, TextureName(images, material, MaterialTexture::ORM, ctx), channels, TextureRole::Linear, ctx);
}

// A plain texture from one image. Alpha is set to 1 so formats without alpha don't warn about it.
static std::expected<std::string, std::string> BuildSingle(const ImportedMaterial& material, SourceSlot source, MaterialTexture slot,
    std::string_view recipe, TextureRole role, MaterialContext& ctx)
{
    const auto image = ResolveSlot(material, source, ctx);
    if (!image)
    {
        return std::string();
    }

    const std::vector images = { &*image };
    return Queue(images, recipe, TextureName(images, material, slot, ctx), kOpaque, role, ctx);
}

// The wrap mode of the first of `slots` that has a texture.
static void SetWrap(ModelFile::Material& out, MaterialTexture target, const ImportedMaterial& material, std::initializer_list<SourceSlot> slots)
{
    for (const SourceSlot slot : slots)
    {
        if (const ImportedTexture& texture = material.Texture(slot))
        {
            out.wrapU[size_t(target)] = texture.wrapU;
            out.wrapV[size_t(target)] = texture.wrapV;
            return;
        }
    }
}

std::expected<BuiltMaterial, std::string> BuildMaterial(const ImportedMaterial& material, MaterialContext& ctx)
{
    BuiltMaterial        built;
    ModelFile::Material& out = built.material;
    ModelFile::AlphaMode alphaMode = material.alphaMode;

    // In MaterialTexture order.
    const std::expected<std::string, std::string> textures[] =
    {
        BuildBaseColor(material, alphaMode, ctx),
        BuildSingle(material, SourceSlot::Normal, MaterialTexture::Normal, "normal", TextureRole::Normal, ctx),
        BuildOrm(material, ctx),
        BuildSingle(material, SourceSlot::Emissive, MaterialTexture::Emissive, "color-opaque", TextureRole::Color, ctx),
        BuildSingle(material, SourceSlot::Transmission, MaterialTexture::Transmission, "mask", TextureRole::Mask, ctx),
    };
    static_assert(std::size(textures) == ModelFile::kMaterialTextureCount);

    for (size_t slot = 0; slot < std::size(textures); ++slot)
    {
        if (!textures[slot])
        {
            return std::unexpected(std::format("material {}: {}", material.name, textures[slot].error()));
        }
        built.textures[slot] = *textures[slot];
    }

    SetWrap(out, MaterialTexture::BaseColor, material, { SourceSlot::BaseColor, SourceSlot::Opacity });
    SetWrap(out, MaterialTexture::Normal, material, { SourceSlot::Normal });
    SetWrap(out, MaterialTexture::ORM, material, { SourceSlot::Roughness, SourceSlot::Metalness, SourceSlot::Occlusion });
    SetWrap(out, MaterialTexture::Emissive, material, { SourceSlot::Emissive });
    SetWrap(out, MaterialTexture::Transmission, material, { SourceSlot::Transmission });

    std::ranges::copy(std::array{ material.baseColor.r, material.baseColor.g, material.baseColor.b, material.baseColor.a }, out.baseColor);
    std::ranges::copy(std::array{ material.emissive.r, material.emissive.g, material.emissive.b }, out.emissive);
    out.emissiveStrength = material.emissiveStrength;
    out.metallic = material.metallic;
    out.roughness = material.roughness;
    out.occlusionStrength = material.occlusionStrength;
    out.normalScale = material.normalScale;
    out.transmission = material.transmission;
    out.ior = material.ior;
    out.alphaCutoff = material.alphaCutoff;
    out.alphaMode = alphaMode;
    out.doubleSided = material.doubleSided ? 1 : 0;
    return built;
}