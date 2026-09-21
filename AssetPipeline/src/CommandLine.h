#pragma once

#include "Textures/TextureRoles.h"

struct Options
{
    std::filesystem::path      input;
    std::optional<TextureRole> role;                        // required when the input contains textures
    std::string                platform = "windows-d3d12";
    GpuTarget                  target = GpuTarget::Desktop;
    std::string                optimize = "balanced";
    std::filesystem::path      output;                      // default: Cooked/<platform>
};

// On failure the error is the process exit code: 0 after --help, 2 for usage errors (already printed).
std::expected<Options, int> ParseCommandLine(int argc, char** argv);