#include <pch.h>
#include "Materials.h"

using ModelFile::MaterialTexture;

// An image with a stable key, ready to become a texture input.
struct ImageRef
{
    std::string key;
    ImageSource source;

    bool operator==(const ImageRef& other) const { return key == other.key; }
};

static std::optional<ImageRef> ResolveImage(const ImportedTexture& texture, MaterialContext& ctx)
{
    if (texture.embedded >= 0)
    {
        return ImageRef{ std::format("{}#embedded/{}", ctx.modelKey, texture.embedded), {.bytes = ctx.scene.embeddedImages[size_t(texture.embedded)] } };
    }
    if (texture.path.empty())
    {
        return std::nullopt;
    }

    auto key = SourceKey(texture.path, ctx.root);
    if (!key)
    {
        ctx.warnings.push_back(std::format("texture skipped: {}", key.error()));
        return std::nullopt;
    }

    ctx.usedImages.insert(*key);
    return ImageRef{ std::move(*key), {.path = texture.path } };
}

static std::optional<ImageRef> ResolveSlot(const ImportedMaterial& material, SourceSlot slot, MaterialContext& ctx)
{
    return ResolveImage(material.Texture(slot), ctx);
}

// Queues a texture and returns its ID.
static std::expected<AssetId, std::string> Queue(std::string name, std::vector<ImageSource> inputs, const ChannelMap& channels, TextureRole role, MaterialContext& ctx)
{
    const AssetId id = MakeAssetId(name);
    TextureRequest request{ .id = id, .name = std::move(name), .inputs = std::move(inputs), .channels = channels, .settings = {.role = role } };
    if (auto added = ctx.textures.Add(std::move(request)); !added)
    {
        return std::unexpected(added.error());
    }
    return id;
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
static std::expected<AssetId, std::string> BuildBaseColor(const ImportedMaterial& material, ModelFile::AlphaMode& alphaMode, MaterialContext& ctx)
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
            return AssetId(0);

        return alphaMode == ModelFile::AlphaMode::Opaque
            ? Queue(color->key + "#color-opaque", { color->source }, kOpaque, TextureRole::Color, ctx)
            : Queue(color->key + "#color", { color->source }, kPassThrough, TextureRole::Color, ctx);
    }

    // Opacity from its own image's first channel; without a color image, color comes from the factor.
    std::vector<ImageSource> inputs;
    ChannelMap               channels = { kOne, kOne, kOne, { 0, 0 } };
    if (color)
    {
        inputs.push_back(color->source);
        channels = { { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 0 } } };
    }
    inputs.push_back(opacity->source);

    const std::string name = std::format("color(rgb={}, a={})", color ? color->key : "-", opacity->key);
    return Queue(name, std::move(inputs), channels, TextureRole::Color, ctx);
}

// Occlusion, roughness and metalness packed into R, G and B. A glTF file already stores roughness
// in G and metalness in B of one image (often with occlusion in its R), so it passes straight
// through; separate grayscale images are read from their first channel. Missing channels are 1,
// which leaves the material factor in charge.
static std::expected<AssetId, std::string> BuildOrm(const ImportedMaterial& material, MaterialContext& ctx)
{
    const auto occlusion = ResolveSlot(material, SourceSlot::Occlusion, ctx);
    const auto roughness = ResolveSlot(material, SourceSlot::Roughness, ctx);
    const auto metalness = ResolveSlot(material, SourceSlot::Metalness, ctx);
    if (!occlusion && !roughness && !metalness)
    {
        return AssetId(0);
    }

    std::vector<ImageSource> inputs;
    std::vector<std::string> inputKeys;
    const auto inputFor = [&](const ImageRef& image) -> int8_t
        {
            if (const auto found = std::ranges::find(inputKeys, image.key); found != inputKeys.end())
            {
                return int8_t(found - inputKeys.begin());
            }
            inputKeys.push_back(image.key);
            inputs.push_back(image.source);
            return int8_t(inputs.size() - 1);
        };

    const bool combined = roughness && roughness == metalness;
    ChannelMap channels = { kOne, kOne, kOne, kOne };
    if (occlusion) channels[0] = { inputFor(*occlusion), 0 };
    if (roughness) channels[1] = { inputFor(*roughness), uint8_t(combined ? 1 : 0) };
    if (metalness) channels[2] = { inputFor(*metalness), uint8_t(combined ? 2 : 0) };

    const auto keyOf = [](const std::optional<ImageRef>& image) { return image ? std::string_view(image->key) : std::string_view("-"); };
    const std::string name = std::format("orm(o={}, r={}, m={})", keyOf(occlusion), keyOf(roughness), keyOf(metalness));
    return Queue(name, std::move(inputs), channels, TextureRole::Linear, ctx);
}

// A plain texture from one image. Alpha is set to 1 so formats without alpha don't warn about it.
static std::expected<AssetId, std::string> BuildSingle(const ImportedMaterial& material, SourceSlot slot, std::string_view tag, TextureRole role, MaterialContext& ctx)
{
    const auto image = ResolveSlot(material, slot, ctx);
    if (!image)
    {
        return AssetId(0);
    }
    return Queue(std::format("{}#{}", image->key, tag), { image->source }, kOpaque, role, ctx);
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

std::expected<ModelFile::Material, std::string> BuildMaterial(const ImportedMaterial& material, MaterialContext& ctx)
{
    ModelFile::Material  out{};
    ModelFile::AlphaMode alphaMode = material.alphaMode;

    // In MaterialTexture order.
    const std::expected<AssetId, std::string> ids[] =
    {
        BuildBaseColor(material, alphaMode, ctx),
        BuildSingle(material, SourceSlot::Normal, "normal", TextureRole::Normal, ctx),
        BuildOrm(material, ctx),
        BuildSingle(material, SourceSlot::Emissive, "color-opaque", TextureRole::Color, ctx),
        BuildSingle(material, SourceSlot::Transmission, "mask", TextureRole::Mask, ctx),
    };
    static_assert(std::size(ids) == ModelFile::kMaterialTextureCount);

    for (size_t slot = 0; slot < std::size(ids); ++slot)
    {
        if (!ids[slot])
        {
            return std::unexpected(std::format("material {}: {}", material.name, ids[slot].error()));
        }
        out.textures[slot] = *ids[slot];
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
    return out;
}