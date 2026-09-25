/*****************************************************************//**
 * \file   CookedFile.h
 * \brief  Reading the files the asset pipeline cooks.
 *********************************************************************/

#pragma once

#include "ModelFormat.h"

// Cooked models (ModelFormat.h) and clips (AnimationFormat.h) share one layout: a header, a
// section table, then the sections' data. Errors are logged.
namespace Radis::CookedFile
{
    bool ReadFile(const std::string& path, std::vector<std::byte>& out);

    // Decodes `section` into `out`, which must hold elementCount elements of elementSize bytes.
    // `name` names the section in errors.
    bool Decode(const ModelFile::Section& section, const std::byte* data, void* out, const std::string& path, const char* name);

    template <typename T>
    bool DecodeArray(const ModelFile::Section& section, const std::byte* data, std::vector<T>& out, const std::string& path, const char* name)
    {
        if (section.elementSize != sizeof(T))
        {
            RADIS_ERROR("{}: {} section has {}-byte elements, expected {}", path, name, section.elementSize, sizeof(T));
            return false;
        }

        out.resize(section.elementCount);
        return Decode(section, data, out.data(), path, name);
    }
}
