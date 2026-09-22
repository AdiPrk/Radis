#pragma once
#include "TextureFormats.h"

enum class TextureRole : uint8_t { Color, Linear, Normal, Mask, Count };
enum class GpuTarget : uint8_t { Desktop, MobileASTC, MobileETC2, Count };

constexpr const char* kRoleNames[] = { "color", "linear", "normal", "mask" };

constexpr TextureFormat kHdrFormats[] = { TextureFormat::BC6H, TextureFormat::ASTC_6x6_HDR, TextureFormat::RGBA16F };
constexpr TextureFormat kRoleFormats[][size_t(GpuTarget::Count)] =
{
    // Desktop                   MobileASTC                    MobileETC2
    { TextureFormat::BC7_SRGB,   TextureFormat::ASTC_6x6_SRGB, TextureFormat::ETC2_RGBA_SRGB }, /* Color  */
    { TextureFormat::BC7,        TextureFormat::ASTC_6x6,      TextureFormat::ETC2_RGBA      }, /* Linear */
    { TextureFormat::BC5,        TextureFormat::ASTC_4x4,      TextureFormat::EAC_RG11       }, /* Normal */
    { TextureFormat::BC4,        TextureFormat::ASTC_6x6,      TextureFormat::EAC_R11        }, /* Mask   */
};

static_assert(std::size(kRoleFormats) == size_t(TextureRole::Count), "kRoleFormats out of sync with TextureRole");
static_assert(std::size(kRoleNames) == size_t(TextureRole::Count), "kRoleNames out of sync with TextureRole");
static_assert(std::size(kHdrFormats) == size_t(GpuTarget::Count), "kHdrFormats out of sync with GpuTarget");

constexpr bool RoleKeepsHdr(TextureRole role)
{
    return role == TextureRole::Color || role == TextureRole::Linear;
}

constexpr TextureFormat ChooseFormat(TextureRole role, bool hdr, GpuTarget target)
{
    if (hdr && RoleKeepsHdr(role))
    {
        return kHdrFormats[size_t(target)];
    }

    return kRoleFormats[size_t(role)][size_t(target)];
}

constexpr std::optional<TextureRole> ParseTextureRole(std::string_view name)
{
    for (size_t i = 0; i < std::size(kRoleNames); ++i)
    {
        if (name == kRoleNames[i]) return TextureRole(i);
    }

    return std::nullopt;
}