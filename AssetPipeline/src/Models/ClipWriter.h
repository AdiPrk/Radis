#pragma once

#include "ClipProcessing.h"

// Writes the clip file described in AnimationFormat.h.
std::expected<void, std::string> WriteClip(const std::filesystem::path& path, const ProcessedClip& clip);
