#pragma once

#include "../AssetId.h"
#include "ImageLoader.h"
#include "TextureEncoding.h"
#include "Mips.h"

// How one texture is processed. Two requests for the same asset must agree on it.
struct TextureSettings
{
    TextureRole role = TextureRole::Color;
    MipSettings mips;

    bool operator==(const TextureSettings&) const = default;
};

// Options shared by every texture in a build.
struct TextureBuildOptions
{
    GpuTarget     target = GpuTarget::Desktop;
    EncodeQuality quality = EncodeQuality::Normal;
    bool          requireAlignedTopMip = false;   // top mip of block formats must be a whole number of blocks (D3D12)
};

// Where one output channel comes from: a channel of one of the request's inputs, or a constant.
struct ChannelSource
{
    static constexpr int8_t kConstant = -1;

    int8_t  input = 0;         // index into TextureRequest::inputs, or kConstant
    uint8_t channel = 0;       // 0..3 = r, g, b, a
    float   constant = 0.0f;   // the final (linear) value, used when input is kConstant

    static constexpr ChannelSource Constant(float value) { return { kConstant, 0, value }; }

    bool operator==(const ChannelSource&) const = default;
};

using ChannelMap = std::array<ChannelSource, 4>;

// RGBA of the first input, unchanged.
inline constexpr ChannelMap kPassThrough = { { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 } } };

// Everything needed to cook one texture asset.
struct TextureRequest
{
    AssetId                  id = 0;
    std::string              name;                     // the source key the ID came from, e.g. "props/crate.glb#orm"
    std::vector<ImageSource> inputs;                   // all the same size
    ChannelMap               channels = kPassThrough;  // how the inputs combine into RGBA
    TextureSettings          settings;
};

struct CookedMip
{
    uint32_t               width = 0;
    uint32_t               height = 0;
    std::vector<std::byte> data;   // blocks in row-major order
};

struct CookedTexture
{
    TextureFormat            format = TextureFormat::Unknown;
    std::vector<CookedMip>   mips;       // largest first
    std::vector<std::string> warnings;   // source data the chosen format couldn't keep
};

// Safe to call from several threads at once.
std::expected<CookedTexture, std::string> CookTexture(const TextureRequest& request, const TextureBuildOptions& options);