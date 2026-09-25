#include <pch.h>
#include "TextureCookQueue.h"
#include "DDS/DdsWriter.h"

static std::string ToLower(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}

// Replaces what Windows doesn't allow in file names; names come from material names, among others.
static std::string SanitizeFileName(std::string name)
{
    for (char& c : name)
    {
        if (static_cast<unsigned char>(c) < 32 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos)
        {
            c = '_';
        }
    }
    while (!name.empty() && (name.back() == '.' || name.back() == ' '))
    {
        name.pop_back();
    }
    return name.empty() ? "Texture" : name;
}

std::string TextureCookQueue::UniqueFileName(const std::filesystem::path& folder, const std::string& name, std::string& note)
{
    const std::string base = SanitizeFileName(name);
    const auto        taken = [&](const std::string& candidate) { return ToLower((folder / candidate).generic_string()); };

    std::string candidate = base;
    for (uint32_t suffix = 2; m_takenNames.contains(taken(candidate)); ++suffix)
    {
        candidate = std::format("{}_{}", base, suffix);
    }

    if (candidate != base)
    {
        note = std::format("named {}.dds because {}.dds is another texture", candidate, base);
    }
    m_takenNames.insert(taken(candidate));
    return candidate + ".dds";
}

std::expected<std::string, std::string> TextureCookQueue::Add(TextureRequest request)
{
    if (const auto found = m_indexByKey.find(request.key); found != m_indexByKey.end())
    {
        const QueuedTexture& queued = m_textures[found->second];
        if (queued.request.settings != request.settings || queued.request.channels != request.channels)
        {
            return std::unexpected(std::format("{} is requested twice with different settings", queued.fileName));
        }
        return queued.fileName;
    }

    QueuedTexture queued;
    queued.fileName = UniqueFileName(request.folder, request.name, queued.nameNote);
    queued.request = std::move(request);

    m_indexByKey.emplace(queued.request.key, m_textures.size());
    m_textures.push_back(std::move(queued));
    return m_textures.back().fileName;
}

static TextureCookResult CookAndWrite(const QueuedTexture& texture, const TextureBuildOptions& options, const std::filesystem::path& outputDir)
{
    const auto start = std::chrono::steady_clock::now();

    TextureCookResult result{ .output = outputDir / texture.request.folder / texture.fileName };
    if (!texture.nameNote.empty())
    {
        result.warnings.push_back(texture.nameNote);
    }

    auto cooked = CookTexture(texture.request, options);
    if (!cooked)
    {
        result.error = std::move(cooked.error());
        return result;
    }

    result.warnings.insert(result.warnings.end(), cooked->warnings.begin(), cooked->warnings.end());   // reported even if writing fails
    if (auto written = WriteDds(result.output, *cooked); !written)
    {
        result.error = std::move(written.error());
        return result;
    }

    const CookedMip& top = cooked->mips.front();
    result.format = cooked->format;
    result.width = top.width;
    result.height = top.height;
    result.mipCount = uint32_t(cooked->mips.size());
    result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}

std::vector<TextureCookResult> TextureCookQueue::Cook(const TextureBuildOptions& options, const std::filesystem::path& outputDir) const
{
    std::vector<TextureCookResult> results;
    results.reserve(m_textures.size());
    for (const QueuedTexture& texture : m_textures)
    {
        results.push_back(CookAndWrite(texture, options, outputDir));
    }
    return results;
}