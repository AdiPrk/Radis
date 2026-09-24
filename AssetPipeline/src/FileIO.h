#pragma once

std::expected<std::vector<std::byte>, std::string> ReadFile(const std::filesystem::path& path);

// Writes `parts` one after another to `path`. The data goes to a temporary file next to the target
// that is then renamed over it, so a failed or interrupted write never leaves a truncated file.
std::expected<void, std::string> WriteFileAtomic(const std::filesystem::path& path, std::span<const std::span<const std::byte>> parts);

inline std::expected<void, std::string> WriteFileAtomic(const std::filesystem::path& path, std::span<const std::byte> data)
{
    return WriteFileAtomic(path, std::span(&data, 1));
}