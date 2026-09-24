#include <pch.h>
#include "ModelDump.h"
#include "ModelFormat.h"
#include "../FileIO.h"

#include <meshoptimizer.h>

using namespace ModelFile;

struct LoadedModel
{
    Header                        header{};
    std::vector<Section>          sections;
    std::vector<Submesh>          submeshes;
    std::vector<Material>         materials;
    std::vector<Position>         positions;
    std::vector<VertexAttributes> attributes;
    std::vector<uint32_t>         indices;
};

static constexpr const char* kSectionNames[] = { "Submeshes", "Materials", "Positions", "Attributes", "Indices" };
static constexpr const char* kCodecNames[] = { "none", "meshopt vertex", "meshopt index" };
static constexpr const char* kAlphaModeNames[] = { "opaque", "mask", "blend" };
static constexpr const char* kWrapNames[] = { "repeat", "clamp", "mirror" };
static constexpr const char* kTextureNames[] = { "BaseColor", "Normal", "ORM", "Emissive", "Transmission" };
static_assert(std::size(kTextureNames) == kMaterialTextureCount);

template <size_t N, typename T>
static const char* NameOf(const char* const (&names)[N], T value)
{
    return size_t(value) < N ? names[size_t(value)] : "?";
}

// Decodes a section of T elements (for indices, T is the widened type and elementSize may be 2).
template <typename T>
static std::expected<std::vector<T>, std::string> Decode(const Section& section, std::span<const std::byte> file)
{
    const std::byte* data = file.data() + section.offset;
    const size_t     decodedSize = size_t(section.elementSize) * section.elementCount;

    std::vector<std::byte> decoded(decodedSize);
    int result = 0;
    switch (section.codec)
    {
    case Codec::None:
        if (section.size != decodedSize) return std::unexpected("stored size doesn't match its elements");
        std::memcpy(decoded.data(), data, decodedSize);
        break;
    case Codec::MeshoptVertex:
        result = meshopt_decodeVertexBuffer(decoded.data(), section.elementCount, section.elementSize,
            reinterpret_cast<const unsigned char*>(data), size_t(section.size));
        break;
    case Codec::MeshoptIndex:
        result = meshopt_decodeIndexBuffer(decoded.data(), section.elementCount, section.elementSize,
            reinterpret_cast<const unsigned char*>(data), size_t(section.size));
        break;
    default:
        return std::unexpected("unknown codec");
    }
    if (result != 0)
    {
        return std::unexpected(std::format("decoding failed ({})", result));
    }

    std::vector<T> out(section.elementCount);
    if (section.elementSize == sizeof(T))
    {
        std::memcpy(out.data(), decoded.data(), decodedSize);
    }
    else if constexpr (std::is_integral_v<T>)
    {
        if (section.elementSize != 2) return std::unexpected("indices must be 2 or 4 bytes");
        const auto* narrow = reinterpret_cast<const uint16_t*>(decoded.data());
        std::copy(narrow, narrow + section.elementCount, out.begin());
    }
    else
    {
        return std::unexpected(std::format("element size is {}, expected {}", section.elementSize, sizeof(T)));
    }
    return out;
}

static std::expected<LoadedModel, std::string> Load(std::span<const std::byte> file)
{
    LoadedModel model;
    if (file.size() < sizeof(Header))
        return std::unexpected("file is too small");

    std::memcpy(&model.header, file.data(), sizeof(Header));
    if (model.header.magic != kMagic)
        return std::unexpected("not a cooked model");
    if (model.header.version != kVersion)
        return std::unexpected(std::format("version {}, expected {}; re-cook it", model.header.version, kVersion));

    const size_t tableEnd = sizeof(Header) + sizeof(Section) * model.header.sectionCount;
    if (file.size() < tableEnd)
        return std::unexpected("section table is truncated");

    model.sections.resize(model.header.sectionCount);
    std::memcpy(model.sections.data(), file.data() + sizeof(Header), sizeof(Section) * model.sections.size());

    bool found[std::size(kSectionNames)] = {};
    for (const Section& section : model.sections)
    {
        if (section.offset > file.size() || section.size > file.size() - section.offset)
            return std::unexpected(std::format("{} section lies outside the file", NameOf(kSectionNames, section.type)));

        std::expected<void, std::string> decoded;
        const auto into = [&](auto& vector)
            {
                auto result = Decode<typename std::remove_reference_t<decltype(vector)>::value_type>(section, file);
                if (result) vector = std::move(*result);
                else decoded = std::unexpected(result.error());
            };

        switch (section.type)
        {
        case SectionType::Submeshes:  into(model.submeshes);  break;
        case SectionType::Materials:  into(model.materials);  break;
        case SectionType::Positions:  into(model.positions);  break;
        case SectionType::Attributes: into(model.attributes); break;
        case SectionType::Indices:    into(model.indices);    break;
        default: continue;   // unknown sections are skipped, as a loader would
        }

        if (!decoded)
            return std::unexpected(std::format("{} section: {}", NameOf(kSectionNames, section.type), decoded.error()));
        found[size_t(section.type)] = true;
    }

    for (size_t i = 0; i < std::size(found); ++i)
    {
        if (!found[i])
            return std::unexpected(std::format("{} section is missing", kSectionNames[i]));
    }
    if (model.positions.size() != model.attributes.size())
        return std::unexpected("positions and attributes have different vertex counts");

    for (size_t i = 0; i < model.submeshes.size(); ++i)
    {
        const Submesh& s = model.submeshes[i];
        if (s.material >= model.materials.size()
            || size_t(s.firstIndex) + s.indexCount > model.indices.size()
            || size_t(s.baseVertex) + s.vertexCount > model.positions.size())
        {
            return std::unexpected(std::format("submesh {} refers outside the model", i));
        }

        const auto indices = std::span(model.indices).subspan(s.firstIndex, s.indexCount);
        if (std::ranges::any_of(indices, [&](uint32_t index) { return index >= s.vertexCount; }))
            return std::unexpected(std::format("submesh {} has an index past its vertices", i));
    }
    return model;
}

static void Print(const std::filesystem::path& path, const LoadedModel& model)
{
    const Header& h = model.header;
    std::printf("%s: model %016llx, bounds (%.3f, %.3f, %.3f) to (%.3f, %.3f, %.3f)\n", path.string().c_str(),
        static_cast<unsigned long long>(h.assetId), h.boundsMin[0], h.boundsMin[1], h.boundsMin[2], h.boundsMax[0], h.boundsMax[1], h.boundsMax[2]);

    std::printf("sections:\n");
    for (const Section& s : model.sections)
    {
        const uint64_t decoded = uint64_t(s.elementSize) * s.elementCount;
        std::printf("  %-10s %-14s %7u x %2u bytes  %9llu -> %9llu bytes\n", NameOf(kSectionNames, s.type), NameOf(kCodecNames, s.codec),
            s.elementCount, s.elementSize, static_cast<unsigned long long>(s.size), static_cast<unsigned long long>(decoded));
    }

    std::printf("submeshes:\n");
    for (size_t i = 0; i < model.submeshes.size(); ++i)
    {
        const Submesh& s = model.submeshes[i];
        std::printf("  %zu: material %u, %u triangles, %u vertices\n", i, s.material, s.indexCount / 3, s.vertexCount);
    }

    std::printf("materials:\n");
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const Material& m = model.materials[i];
        const std::string alpha = m.alphaMode == AlphaMode::Mask ? std::format(" (cutoff {:.2f})", m.alphaCutoff) : "";
        std::printf("  %zu: %s%s%s, base color (%.2f, %.2f, %.2f, %.2f), metallic %.2f, roughness %.2f, emissive (%.2f, %.2f, %.2f) x %.2f\n",
            i, NameOf(kAlphaModeNames, m.alphaMode), alpha.c_str(), m.doubleSided ? ", double-sided" : "", m.baseColor[0], m.baseColor[1], m.baseColor[2],
            m.baseColor[3], m.metallic, m.roughness, m.emissive[0], m.emissive[1], m.emissive[2], m.emissiveStrength);

        if (m.transmission > 0.0f)
        {
            std::printf("     transmission %.2f, ior %.2f\n", m.transmission, m.ior);
        }

        for (size_t t = 0; t < kMaterialTextureCount; ++t)
        {
            if (m.textures[t] != 0)
            {
                std::printf("     %-12s %016llx (%s, %s)\n", kTextureNames[t], static_cast<unsigned long long>(m.textures[t]),
                    NameOf(kWrapNames, m.wrapU[t]), NameOf(kWrapNames, m.wrapV[t]));
            }
        }
    }
}

std::expected<void, std::string> DumpModel(const std::filesystem::path& path)
{
    const auto file = ReadFile(path);
    if (!file)
        return std::unexpected(file.error());

    const auto model = Load(*file);
    if (!model)
        return std::unexpected(model.error());

    Print(path, *model);
    return {};
}