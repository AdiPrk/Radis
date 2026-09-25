#include <pch.h>
#include "FileIO.h"

std::expected<std::vector<std::byte>, std::string> ReadFile(const std::filesystem::path& path)
{
    std::ifstream         file(path, std::ios::binary | std::ios::ate);
    const std::streamsize size = file ? std::streamsize(file.tellg()) : -1;
    if (size < 0)
    {
        return std::unexpected("cannot open " + path.string());
    }

    std::vector<std::byte> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
    {
        return std::unexpected("cannot read " + path.string());
    }
    return bytes;
}

std::string PathKey(const std::filesystem::path& path)
{
    std::error_code       ec;
    std::filesystem::path resolved = std::filesystem::weakly_canonical(path, ec);
    if (ec)
    {
        resolved = std::filesystem::absolute(path, ec).lexically_normal();
    }

    const std::u8string text = resolved.generic_u8string();
    std::string         key(text.size(), '\0');
    std::ranges::transform(text, key.begin(), [](char8_t c) { return char(c >= u8'A' && c <= u8'Z' ? c + (u8'a' - u8'A') : c); });
    return key;
}

std::expected<void, std::string> WriteFileAtomic(const std::filesystem::path& path, std::span<const std::span<const std::byte>> parts)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::filesystem::path temp = path;
    temp += ".tmp";

    std::ofstream file(temp, std::ios::binary);
    if (!file)
    {
        return std::unexpected("cannot open " + temp.string());
    }

    for (const std::span<const std::byte> part : parts)
    {
        file.write(reinterpret_cast<const char*>(part.data()), std::streamsize(part.size()));
    }
    file.close();

    if (!file)
    {
        std::filesystem::remove(temp, ec);
        return std::unexpected("failed writing " + temp.string());
    }

    std::filesystem::rename(temp, path, ec);   // replaces an existing file
    if (ec)
    {
        const std::string reason = ec.message();
        std::filesystem::remove(temp, ec);
        return std::unexpected(std::format("cannot replace {}: {}", path.string(), reason));
    }
    return {};
}