#include <pch.h>
#include "Mips.h"

// How a role's data must be treated when pixels are averaged.
struct MipRules
{
    bool alphaIsOpacity = false;   // weight color by alpha so invisible pixels don't bleed into edges
    bool unitVectors = false;   // renormalize xyz after averaging
};

static MipRules RulesFor(TextureRole role)
{
    switch (role)
    {
    case TextureRole::Color:  return { .alphaIsOpacity = true };
    case TextureRole::Normal: return { .unitVectors = true };
    default:                  return {};
    }
}

static void Average(const float* const (&taps)[4], const MipRules& rules, float* out)
{
    float alpha = 0.0f;
    for (const float* tap : taps)
    {
        alpha += tap[3];
    }

    // Weighted by alpha unless every tap is fully transparent.
    const bool weighted = rules.alphaIsOpacity && alpha > 0.0f;
    for (size_t c = 0; c < 3; ++c)
    {
        float sum = 0.0f;
        for (const float* tap : taps)
        {
            sum += weighted ? tap[c] * tap[3] : tap[c];
        }
        out[c] = weighted ? sum / alpha : sum * 0.25f;
    }
    out[3] = alpha * 0.25f;

    if (rules.unitVectors)
    {
        NormalizeVector(out);
    }
}

static LinearImage DownsampleBox(const LinearImage& src, const MipRules& rules)
{
    LinearImage dst{ std::max(1u, src.width / 2), std::max(1u, src.height / 2), {} };
    dst.pixels.resize(size_t(dst.width) * dst.height * 4);

    // TODO: odd sizes drop the last source row/column; an exact filter would weight it in.
    for (uint32_t y = 0; y < dst.height; ++y)
    {
        const float* row0 = src.Row(std::min(y * 2, src.height - 1));
        const float* row1 = src.Row(std::min(y * 2 + 1, src.height - 1));
        float* out = dst.Row(y);

        for (uint32_t x = 0; x < dst.width; ++x)
        {
            const size_t x0 = size_t(std::min(x * 2, src.width - 1)) * 4;
            const size_t x1 = size_t(std::min(x * 2 + 1, src.width - 1)) * 4;

            const float* const taps[4] = { row0 + x0, row0 + x1, row1 + x0, row1 + x1 };
            Average(taps, rules, out + size_t(x) * 4);
        }
    }
    return dst;
}

static LinearImage Downsample(const LinearImage& src, const MipRules& rules, MipFilter filter)
{
    switch (filter)
    {
    case MipFilter::Box: return DownsampleBox(src, rules);
    }
    return DownsampleBox(src, rules);
}

static bool Fits(const LinearImage& image, uint32_t maxSize)
{
    return maxSize == 0 || std::max(image.width, image.height) <= maxSize;
}

std::expected<void, std::string> ForEachMip(LinearImage base, TextureRole role, const MipSettings& settings, const MipVisitor& visit)
{
    const MipRules rules = RulesFor(role);

    // Levels above the size cap are computed but never visited.
    while (!Fits(base, settings.maxSize))
    {
        base = Downsample(base, rules, settings.filter);
    }

    LinearImage level = std::move(base);
    for (uint32_t index = 0; ; ++index)
    {
        if (auto result = visit(level, index); !result)
        {
            return result;
        }

        if (!settings.generate || (level.width == 1 && level.height == 1))
        {
            return {};
        }

        level = Downsample(level, rules, settings.filter);
    }
}