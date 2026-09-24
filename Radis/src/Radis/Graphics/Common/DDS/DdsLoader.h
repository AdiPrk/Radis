/*****************************************************************//**
 * \file   DdsLoader.h
 * \brief  Loads DDS textures cooked by the asset pipeline.
 *********************************************************************/

#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Radis
{
    // Reads DDS files as the asset pipeline writes them (see DdsFormat.h): a DX10 header and one 2D
    // image with its mip chain, largest mip first. The mips are uploaded exactly as stored, so the
    // texture is marked isCompressed ("has prebuilt mips") even for the uncompressed formats.
    class DdsLoader
    {
    public:
        static bool FromFile(const std::string& path, TextureData& outTexture);
        static bool FromMemory(const unsigned char* data, size_t size, const std::string& name, TextureData& outTexture);

        // Whether `data` starts with the DDS magic.
        static bool IsDds(const unsigned char* data, size_t size);
    };
}