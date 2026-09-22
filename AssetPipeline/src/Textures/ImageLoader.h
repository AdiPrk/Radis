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

// Loads any supported image as RGBA: F32 for HDR sources (.hdr, .exr), U16 for 16-bit PNGs, U8 otherwise.
std::expected<SourceImage, std::string> LoadSourceImage(const std::filesystem::path& path);