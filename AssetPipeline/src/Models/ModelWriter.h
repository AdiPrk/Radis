#pragma once

#include "MeshProcessing.h"
#include "../AssetId.h"

// Writes the model file described in ModelFormat.h.
std::expected<void, std::string> WriteModel(const std::filesystem::path& path, AssetId id, const ProcessedGeometry& geometry,
    std::span<const ModelFile::Material> materials);