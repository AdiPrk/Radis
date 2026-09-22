#pragma once

#include "Textures/TextureEncoding.h"

struct PlatformInfo
{
    std::string_view name;
    GpuTarget        target;
    bool             requireAlignedTopMip;   // D3D12 rejects block-compressed textures with a partial-block top mip
};

inline constexpr PlatformInfo kPlatforms[] =
{
    { "windows-d3d12",  GpuTarget::Desktop,    true  },
    { "windows-vulkan", GpuTarget::Desktop,    false },
    { "linux-vulkan",   GpuTarget::Desktop,    false },
    { "android-vulkan", GpuTarget::MobileASTC, false },
};

struct Options
{
    std::filesystem::path      input;
    std::optional<TextureRole> role;                        // required when the input contains textures
    PlatformInfo               platform = kPlatforms[0];
    EncodeQuality              quality = EncodeQuality::Normal;
    std::filesystem::path      output;                      // default: Cooked/<platform>
};

// On failure the error is the process exit code: 0 after --help, 2 for usage errors (already printed).
std::expected<Options, int> ParseCommandLine(int argc, char** argv);