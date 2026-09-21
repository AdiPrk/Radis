#pragma once

#include "TextureFormats.h"
#include "TextureRoles.h"
#include "TextureEncoding.h"

struct CookedTexture
{
    TextureFormat          format = TextureFormat::Unknown;
    uint32_t               width = 0;
    uint32_t               height = 0;
    std::vector<std::byte> data;   // mip 0 only, blocks in row-major order
};

struct TextureCookSettings
{
    TextureRole   role = TextureRole::Color;
    GpuTarget     target = GpuTarget::Desktop;
    EncodeQuality quality = EncodeQuality::Normal;
};

std::expected<CookedTexture, std::string> CookTexture(const std::filesystem::path& path, const TextureCookSettings& settings);
std::expected<void, std::string> WriteDds(const std::filesystem::path& path, const CookedTexture& texture);