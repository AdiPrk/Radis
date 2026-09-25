/*****************************************************************//**
 * \file   CookedFile.cpp
 * \brief  Reading the files the asset pipeline cooks.
 *********************************************************************/

#include <PCH/pch.h>
#include "CookedFile.h"

#include <meshoptimizer.h>

namespace Radis::CookedFile
{
    bool ReadFile(const std::string& path, std::vector<std::byte>& out)
    {
        std::ifstream         file(path, std::ios::binary | std::ios::ate);
        const std::streamsize size = file ? std::streamsize(file.tellg()) : -1;
        if (size < 0)
        {
            RADIS_ERROR("{}: cannot open", path);
            return false;
        }

        out.resize(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(out.data()), size))
        {
            RADIS_ERROR("{}: cannot read", path);
            return false;
        }
        return true;
    }

    bool Decode(const ModelFile::Section& section, const std::byte* data, void* out, const std::string& path, const char* name)
    {
        const size_t decodedSize = size_t(section.elementSize) * section.elementCount;
        const auto* encoded = reinterpret_cast<const unsigned char*>(data);

        int result = 0;
        switch (section.codec)
        {
        case ModelFile::Codec::None:
            if (section.size != decodedSize)
            {
                RADIS_ERROR("{}: {} section holds {} bytes where {} are needed", path, name, section.size, decodedSize);
                return false;
            }
            if (decodedSize > 0)   // an empty section (a model without textures has no strings) may have no buffer
            {
                std::memcpy(out, data, decodedSize);
            }
            break;

        case ModelFile::Codec::MeshoptVertex:
            result = meshopt_decodeVertexBuffer(out, section.elementCount, section.elementSize, encoded, size_t(section.size));
            break;

        case ModelFile::Codec::MeshoptIndex:
            result = meshopt_decodeIndexBuffer(out, section.elementCount, section.elementSize, encoded, size_t(section.size));
            break;

        default:
            RADIS_ERROR("{}: {} section uses an unknown codec", path, name);
            return false;
        }

        if (result != 0)
        {
            RADIS_ERROR("{}: decoding the {} section failed ({})", path, name, result);
            return false;
        }
        return true;
    }
}
