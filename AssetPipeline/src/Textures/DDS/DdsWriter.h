#pragma once

#include "../Textures.h"

// Debug output: writes a cooked texture as .dds for viewing in external tools.
std::expected<void, std::string> WriteDds(const std::filesystem::path& path, const CookedTexture& texture);