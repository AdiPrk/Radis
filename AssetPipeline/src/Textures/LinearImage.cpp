#include <pch.h>
#include "LinearImage.h"

static float SrgbToLinear(float c)
{
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

static float LinearToSrgb(float c)
{
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

static float Saturate(float v)
{
    return v > 0.0f ? (v < 1.0f ? v : 1.0f) : 0.0f;
}

static uint8_t ToUnorm8(float v)
{
    return uint8_t(Saturate(v) * 255.0f + 0.5f);
}

// sRGB -> linear, one entry per code of an integer channel.
static std::vector<float> MakeSrgbToLinearTable(size_t codes)
{
    std::vector<float> table(codes);
    for (size_t i = 0; i < codes; ++i)
    {
        table[i] = SrgbToLinear(float(i) / float(codes - 1));
    }
    return table;
}

static const std::vector<float> kSrgb8ToLinear = MakeSrgbToLinearTable(256);

// Linear -> 8-bit sRGB, indexed by the linear value quantized to 16 bits
// Matches the exact formula except for values within 1/65535 of a rounding boundary.
static constexpr size_t kLinearToSrgbSize = 65536;

static const std::array<uint8_t, kLinearToSrgbSize> kLinearToSrgb = []
    {
        std::array<uint8_t, kLinearToSrgbSize> table{};
        for (size_t i = 0; i < table.size(); ++i)
        {
            table[i] = ToUnorm8(LinearToSrgb(float(i) / float(kLinearToSrgbSize - 1)));
        }
        return table;
    }();

static uint8_t LinearToSrgb8(float v)
{
    return kLinearToSrgb[size_t(Saturate(v) * float(kLinearToSrgbSize - 1) + 0.5f)];
}

// One channel of integer RGBA -> float. `srgbToLinear` decodes sRGB; empty for linear data.
template <typename T>
static void DecodeUnormChannel(const T* src, uint32_t from, std::span<const float> srgbToLinear, LinearImage& image, uint32_t to)
{
    constexpr float kMax = float(std::numeric_limits<T>::max());
    const size_t    count = image.pixels.size();
    for (size_t i = 0; i < count; i += 4)
    {
        const T value = src[i + from];
        image.pixels[i + to] = srgbToLinear.empty() ? float(value) / kMax : srgbToLinear[value];
    }
}

void NormalizeVector(float* xyz)
{
    const float length = std::sqrt(xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2]);
    if (length > 1e-6f)   // also false for NaN
    {
        xyz[0] /= length;
        xyz[1] /= length;
        xyz[2] /= length;
    }
    else
    {
        xyz[0] = 0.0f;
        xyz[1] = 0.0f;
        xyz[2] = 1.0f;
    }
}

void DecodeChannel(const SourceImage& source, uint32_t from, bool srgb, LinearImage& image, uint32_t to)
{
    switch (source.type)
    {
    case PixelType::U8:
    {
        DecodeUnormChannel(static_cast<const uint8_t*>(source.pixels.get()), from, srgb ? kSrgb8ToLinear : std::span<const float>(), image, to);
        break;
    }
    case PixelType::U16:
    {
        static const std::vector<float> kSrgb16ToLinear = MakeSrgbToLinearTable(65536);   // built on first use
        DecodeUnormChannel(static_cast<const uint16_t*>(source.pixels.get()), from, srgb ? kSrgb16ToLinear : std::span<const float>(), image, to);
        break;
    }
    case PixelType::F32:
    {
        const auto* src = static_cast<const float*>(source.pixels.get());
        const size_t count = image.pixels.size();
        for (size_t i = 0; i < count; i += 4)
        {
            const float value = src[i + from];
            image.pixels[i + to] = std::isnan(value) ? 0.0f : value;
        }
        break;
    }
    }
}

void FillChannel(LinearImage& image, uint32_t channel, float value)
{
    for (size_t i = channel; i < image.pixels.size(); i += 4)
    {
        image.pixels[i] = value;
    }
}

void DecodeNormals(LinearImage& image, bool floatSource)
{
    const size_t count = image.pixels.size();

    // Integer maps always store n * 0.5 + 0.5. Float maps may hold raw [-1, 1] vectors instead:
    // encoded maps average about 0.5 in X and Y, raw vectors about 0.
    bool rawVectors = false;
    if (floatSource)
    {
        double sum = 0.0;
        for (size_t i = 0; i < count; i += 4)
        {
            sum += double(image.pixels[i]) + image.pixels[i + 1];
        }
        rawVectors = sum / double(count / 2) < 0.25;
    }

    for (size_t i = 0; i < count; i += 4)
    {
        float* n = image.pixels.data() + i;
        if (!rawVectors)
        {
            n[0] = n[0] * 2.0f - 1.0f;
            n[1] = n[1] * 2.0f - 1.0f;
            n[2] = n[2] * 2.0f - 1.0f;
        }
        NormalizeVector(n);
    }
}

void ToEncoderPixels(const LinearImage& image, TextureRole role, TextureFormat format, std::vector<std::byte>& out)
{
    const FormatInfo& info = GetFormatInfo(format);
    const size_t      count = image.pixels.size();

    if (info.hdr)
    {
        // Unsigned half floats (BC6H UF16): no negatives, nothing above 65504.
        out.resize(count * sizeof(float));
        auto* dst = reinterpret_cast<float*>(out.data());
        for (size_t i = 0; i < count; ++i)
        {
            const float v = image.pixels[i];
            dst[i] = std::isnan(v) ? 0.0f : std::clamp(v, 0.0f, 65504.0f);
        }
        return;
    }

    const bool normal = role == TextureRole::Normal;
    const bool srgb = info.srgb;

    out.resize(count);
    auto* dst = reinterpret_cast<uint8_t*>(out.data());
    for (size_t i = 0; i < count; i += 4)
    {
        const float* p = image.pixels.data() + i;
        for (size_t c = 0; c < 3; ++c)
        {
            if (normal)
            {
                dst[i + c] = ToUnorm8(p[c] * 0.5f + 0.5f);
            }
            else if (srgb)
            {
                dst[i + c] = LinearToSrgb8(p[c]);
            }
            else
            {
                dst[i + c] = ToUnorm8(p[c]);
            }
        }
        dst[i + 3] = ToUnorm8(p[3]);   // alpha is always linear
    }
}