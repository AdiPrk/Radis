#pragma once

using PixelBuffer = std::unique_ptr<void, void (*)(void*)>;

enum class PixelType : uint8_t { U8, U16, F32 };

struct SourceImage
{
    uint32_t    width = 0;
    uint32_t    height = 0;
    PixelType   type = PixelType::U8;       // always 4 channels (RGBA)
    PixelBuffer pixels{ nullptr, nullptr }; // freed with whatever allocated it
};

// An encoded image (PNG, JPEG, TGA, HDR, EXR) on disk, or already in memory, e.g. embedded in a .glb.
struct ImageSource
{
    std::filesystem::path  path;    // read from disk when `bytes` is empty
    std::vector<std::byte> bytes;   // the encoded file contents
};

// Decodes any supported image as RGBA: F32 for HDR sources (.hdr, .exr), U16 for 16-bit PNGs, U8 otherwise.
std::expected<SourceImage, std::string> LoadSourceImage(std::span<const std::byte> encoded);
std::expected<SourceImage, std::string> LoadSourceImage(const ImageSource& source);

// How an image's alpha is used. Cutout: mostly fully clear or fully opaque (edges may be soft).
// Translucent: more partially transparent pixels than fully clear ones.
enum class AlphaUsage : uint8_t { Opaque, Cutout, Translucent };

// Images whose file has no alpha channel are recognized from the header, without decoding.
std::expected<AlphaUsage, std::string> MeasureAlpha(const ImageSource& source);