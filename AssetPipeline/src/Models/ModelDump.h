#pragma once

// Prints a cooked model's header, sections, submeshes and materials after decoding every section
// and checking them against each other. Also serves as a reference for the engine's loader.
// Given a clip, prints its tracks, and checks it against its model when that's where the output
// layout puts it.
std::expected<void, std::string> DumpModel(const std::filesystem::path& path);