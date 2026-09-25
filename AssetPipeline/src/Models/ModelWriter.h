#pragma once

#include "Materials.h"
#include "MeshProcessing.h"

// Writes the model file described in ModelFormat.h.
std::expected<void, std::string> WriteModel(const std::filesystem::path& path, const ProcessedGeometry& geometry, std::span<const BuiltMaterial> materials,
    const ImportedSkeleton& skeleton);