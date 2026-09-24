#include <pch.h>
#include "TextureCookQueue.h"
#include "DDS/DdsWriter.h"

std::filesystem::path TextureOutputPath(const std::filesystem::path& outputDir, AssetId id)
{
    return outputDir / "textures" / std::format("{:016x}.dds", id);
}

std::expected<void, std::string> TextureCookQueue::Add(TextureRequest request)
{
    const auto [it, inserted] = m_indexById.try_emplace(request.id, m_requests.size());
    if (inserted)
    {
        m_requests.push_back(std::move(request));
        return {};
    }

    const TextureRequest& queued = m_requests[it->second];
    if (queued.name != request.name)
    {
        return std::unexpected(std::format("{} and {} have the same asset ID {:016x}", queued.name, request.name, request.id));
    }

    if (queued.settings != request.settings || queued.channels != request.channels)
    {
        return std::unexpected(std::format("{} is requested twice with different settings", request.name));
    }
    return {};
}

static TextureCookResult CookAndWrite(const TextureRequest& request, const TextureBuildOptions& options, const std::filesystem::path& outputDir)
{
    const auto start = std::chrono::steady_clock::now();

    TextureCookResult result{ .output = TextureOutputPath(outputDir, request.id) };
    auto texture = CookTexture(request, options);
    if (!texture)
    {
        result.error = std::move(texture.error());
        return result;
    }

    result.warnings = std::move(texture->warnings);   // reported even if writing fails
    if (auto written = WriteDds(result.output, *texture); !written)
    {
        result.error = std::move(written.error());
        return result;
    }

    const CookedMip& top = texture->mips.front();
    result.format = texture->format;
    result.width = top.width;
    result.height = top.height;
    result.mipCount = uint32_t(texture->mips.size());
    result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}

std::vector<TextureCookResult> TextureCookQueue::Cook(const TextureBuildOptions& options, const std::filesystem::path& outputDir) const
{
    std::vector<TextureCookResult> results;
    results.reserve(m_requests.size());
    for (const TextureRequest& request : m_requests)
    {
        results.push_back(CookAndWrite(request, options, outputDir));
    }
    return results;
}