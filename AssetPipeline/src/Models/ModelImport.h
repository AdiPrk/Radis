#pragma once

#include "ImportedScene.h"

// Reads a model file with assimp. Warnings (missing textures, unsupported features) go into
// ImportedScene::warnings; an error means nothing usable could be read.
std::expected<ImportedScene, std::string> ImportModel(const std::filesystem::path& path);

// Reads only the animations of a file, such as a clip file in a model's folder.
std::expected<ImportedAnimations, std::string> ImportAnimations(const std::filesystem::path& path, std::vector<std::string>& warnings);