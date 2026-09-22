#pragma once

#include "TextureEncoding.h"
#include "Mips.h"

struct TextureCookSettings
{
    TextureRole   role = TextureRole::Color;
    GpuTarget     target = GpuTarget::Desktop;
    EncodeQuality quality = EncodeQuality::Normal;
    MipSettings   mips;
};

struct CookedMip
{
    uint32_t               width = 0;
    uint32_t               height = 0;
    std::vector<std::byte> data;   // blocks in row-major order
};

struct CookedTexture
{
    TextureFormat          format = TextureFormat::Unknown;
    std::vector<CookedMip> mips;   // largest first
};

std::expected<CookedTexture, std::string> CookTexture(const std::filesystem::path& path, const TextureCookSettings& settings);