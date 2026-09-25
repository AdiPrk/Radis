#pragma once

enum class AssetKind { Texture, Model };

struct InputFile
{
    std::filesystem::path path;       // as found on disk
    std::filesystem::path relative;   // relative to the input directory; just the file name for a single file
    AssetKind             kind;
};

bool IsModelFile(const std::filesystem::path& path);

// A single file, or every supported file under a directory (recursive), sorted by path.
// `exclude` (normally the output directory) is skipped so cooked files never become inputs.
std::expected<std::vector<InputFile>, std::string> CollectInputs(const std::filesystem::path& input, const std::filesystem::path& exclude);