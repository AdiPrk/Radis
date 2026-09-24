#pragma once

#include "ImportedScene.h"

// Reads a model file with assimp. Warnings (missing textures, unsupported features) go into
// ImportedScene::warnings; an error means nothing usable could be read.
std::expected<ImportedScene, std::string> ImportModel(const std::filesystem::path& path);