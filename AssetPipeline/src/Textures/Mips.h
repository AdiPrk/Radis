#pragma once

#include "LinearImage.h"

enum class MipFilter : uint8_t
{
    Box,   // 2x2 average. TODO: Kaiser or Lanczos for sharper distant mips
};

struct MipSettings
{
    bool      generate = true;            // false: a single level (e.g. UI drawn at 1:1)
    MipFilter filter = MipFilter::Box;
    uint32_t  maxSize = 0;               // largest allowed dimension; 0 = no limit
};

// Receives each level, largest first. `index` is 0 for the top kept level.
using MipVisitor = std::function<std::expected<void, std::string>(const LinearImage& level, uint32_t index)>;

// Walks the mip chain of `base`, holding at most two levels in memory. Levels larger than
// maxSize are computed but not visited, so a capped texture's top level is an ordinary mip
// of the full-size source. Stops at the first error `visit` returns.
std::expected<void, std::string> ForEachMip(LinearImage base, TextureRole role, const MipSettings& settings, const MipVisitor& visit);