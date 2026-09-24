#pragma once

// Prints a cooked model's header, sections, submeshes and materials after decoding every section
// and checking them against each other. Also serves as a reference for the engine's loader.
std::expected<void, std::string> DumpModel(const std::filesystem::path& path);