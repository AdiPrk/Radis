#include <pch.h>
#include "Dilate.h"

// Push-pull: "pull" builds coarser and coarser levels from the visible color only, until one
// texel covers the whole image. "Push" then walks back down, and every texel with no visible
// color takes it from the level above. Every hole is filled in O(pixels), however large it is,
// and the filled color fades smoothly from the surrounding edges.
//
// In each pyramid level, rgb is plain (not premultiplied) color and alpha is coverage: how much
// of the area under that texel is visible. In the image itself, coverage is simply alpha.

static float Coverage(const float* texel)
{
    return std::clamp(texel[3], 0.0f, 1.0f);
}

static LinearImage Pull(const LinearImage& fine)
{
    LinearImage coarse{ (fine.width + 1) / 2, (fine.height + 1) / 2, {} };
    coarse.pixels.resize(size_t(coarse.width) * coarse.height * 4);

    for (uint32_t y = 0; y < coarse.height; ++y)
    {
        const float* row0 = fine.Row(y * 2);
        const float* row1 = fine.Row(std::min(y * 2 + 1, fine.height - 1));
        float* out = coarse.Row(y);

        for (uint32_t x = 0; x < coarse.width; ++x)
        {
            const size_t       x0 = size_t(x * 2) * 4;
            const size_t       x1 = size_t(std::min(x * 2 + 1, fine.width - 1)) * 4;
            const float* const taps[4] = { row0 + x0, row0 + x1, row1 + x0, row1 + x1 };

            float rgb[3] = {};
            float coverage = 0.0f;
            for (const float* tap : taps)
            {
                const float weight = Coverage(tap);
                for (size_t c = 0; c < 3; ++c)
                {
                    rgb[c] += tap[c] * weight;
                }
                coverage += weight;
            }

            float* texel = out + size_t(x) * 4;
            for (size_t c = 0; c < 3; ++c)
            {
                texel[c] = coverage > 0.0f ? rgb[c] / coverage : 0.0f;
            }
            texel[3] = coverage * 0.25f;
        }
    }
    return coarse;
}

// Fills the color of every texel in `fine` that has no coverage by sampling `coarse` bilinearly.
static void Push(const LinearImage& coarse, LinearImage& fine)
{
    const float scaleX = float(coarse.width) / float(fine.width);
    const float scaleY = float(coarse.height) / float(fine.height);

    for (uint32_t y = 0; y < fine.height; ++y)
    {
        // Texel centers line up: fine center (y + 0.5) lands at coarse (y + 0.5) * scale.
        const float    sy = std::max((float(y) + 0.5f) * scaleY - 0.5f, 0.0f);
        const uint32_t y0 = std::min(uint32_t(sy), coarse.height - 1);
        const uint32_t y1 = std::min(y0 + 1, coarse.height - 1);
        const float    fy = std::min(sy - float(y0), 1.0f);
        const float* row0 = coarse.Row(y0);
        const float* row1 = coarse.Row(y1);
        float* out = fine.Row(y);

        for (uint32_t x = 0; x < fine.width; ++x)
        {
            float* texel = out + size_t(x) * 4;
            if (Coverage(texel) > 0.0f)
                continue;

            const float    sx = std::max((float(x) + 0.5f) * scaleX - 0.5f, 0.0f);
            const uint32_t x0 = std::min(uint32_t(sx), coarse.width - 1);
            const uint32_t x1 = std::min(x0 + 1, coarse.width - 1);
            const float    fx = std::min(sx - float(x0), 1.0f);

            for (size_t c = 0; c < 3; ++c)
            {
                const float top = std::lerp(row0[size_t(x0) * 4 + c], row0[size_t(x1) * 4 + c], fx);
                const float bottom = std::lerp(row1[size_t(x0) * 4 + c], row1[size_t(x1) * 4 + c], fx);
                texel[c] = std::lerp(top, bottom, fy);
            }
        }
    }
}

void DilateTransparentTexels(LinearImage& image)
{
    // Levels from half size down to 1x1.
    std::vector<LinearImage> pyramid;
    pyramid.push_back(Pull(image));
    while (pyramid.back().width > 1 || pyramid.back().height > 1)
    {
        pyramid.push_back(Pull(pyramid.back()));
    }

    // Nothing is visible, so there's no color to spread.
    if (Coverage(pyramid.back().pixels.data()) <= 0.0f)
        return;

    for (size_t i = pyramid.size() - 1; i-- > 0;)
    {
        Push(pyramid[i + 1], pyramid[i]);
    }
    Push(pyramid.front(), image);
}