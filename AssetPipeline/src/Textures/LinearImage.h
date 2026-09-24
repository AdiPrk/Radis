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

// Decodes channel `from` of `source` into channel `to` of `image`, which must be the same size.
// Integers map to 0..1 (sRGB-decoded when `srgb`); floats are copied with NaN replaced by 0.
void DecodeChannel(const SourceImage& source, uint32_t from, bool srgb, LinearImage& image, uint32_t to);

void FillChannel(LinearImage& image, uint32_t channel, float value);

// Turns stored normals into unit vectors: integer sources always hold n * 0.5 + 0.5, float
// sources may hold either that or raw [-1, 1] vectors, which is detected from the data.
void DecodeNormals(LinearImage& image, bool floatSource);

// Writes encoder input into `out` (resized as needed, so it can be reused across levels):
// RGBA32F for HDR formats, RGBA8 otherwise, sRGB-encoded for sRGB formats.
void ToEncoderPixels(const LinearImage& image, TextureRole role, TextureFormat format, std::vector<std::byte>& out);

// Normalizes xyz in place; a zero-length (or NaN) vector becomes flat (0, 0, 1).
void NormalizeVector(float* xyz);