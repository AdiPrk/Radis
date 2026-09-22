#pragma once

#include "ImageLoader.h"
#include "TextureRoles.h"

// Working representation between loading and encoding: RGBA32F in linear space.
// Color is linear light, normals are unit vectors in [-1, 1], everything else is stored as-is.
struct LinearImage
{
    uint32_t           width = 0;
    uint32_t           height = 0;
    std::vector<float> pixels;   // RGBA, row-major

    float* Row(uint32_t y) { return pixels.data() + size_t(y) * width * 4; }
    const float* Row(uint32_t y) const { return pixels.data() + size_t(y) * width * 4; }
};

LinearImage ToLinearImage(const SourceImage& source, TextureRole role);

// Writes encoder input into `out` (resized as needed, so it can be reused across levels):
// RGBA32F for HDR formats, RGBA8 otherwise, sRGB-encoded for sRGB formats.
void ToEncoderPixels(const LinearImage& image, TextureRole role, TextureFormat format, std::vector<std::byte>& out);

// Normalizes xyz in place; a zero-length (or NaN) vector becomes flat (0, 0, 1).
void NormalizeVector(float* xyz);