#pragma once

#include "TextureEncoding.h"

using PixelBuffer = std::unique_ptr<void, void (*)(void*)>;

struct SourceImage
{
    uint32_t    width = 0;
    uint32_t    height = 0;
    bool        hdr = false;                // true: RGBA32F, false: RGBA8
    PixelBuffer pixels{ nullptr, nullptr }; // freed with whatever allocated it

    ImageView View() const;
};

// Loads any supported image as RGBA: RGBA32F for HDR sources (.hdr, .exr), RGBA8 otherwise.
std::expected<SourceImage, std::string> LoadSourceImage(const std::filesystem::path& path);