#include <pch.h>
#include "ModelWriter.h"
#include "../FileIO.h"

#include <meshoptimizer.h>

// Builds the file in memory: the header and section table are filled in as sections are added.
class ModelFileBuilder
{
public:
    explicit ModelFileBuilder(uint16_t sectionCount)
        : m_sectionCount(sectionCount)
        , m_bytes(sizeof(ModelFile::Header) + sizeof(ModelFile::Section) * sectionCount)
    {
    }

    void Add(ModelFile::SectionType type, ModelFile::Codec codec, uint32_t elementSize, uint32_t elementCount, std::span<const std::byte> data)
    {
        assert(m_added < m_sectionCount);
        m_bytes.resize(AlignUp(m_bytes.size(), ModelFile::kSectionAlignment));

        const ModelFile::Section section{ type, codec, elementSize, elementCount, m_bytes.size(), data.size() };
        std::memcpy(m_bytes.data() + sizeof(ModelFile::Header) + sizeof(ModelFile::Section) * m_added++, &section, sizeof(section));
        m_bytes.insert(m_bytes.end(), data.begin(), data.end());
    }

    template <typename T>
    void AddRaw(ModelFile::SectionType type, std::span<const T> elements)
    {
        Add(type, ModelFile::Codec::None, sizeof(T), uint32_t(elements.size()), std::as_bytes(elements));
    }

    template <typename T>
    void AddVertices(ModelFile::SectionType type, std::span<const T> vertices)
    {
        std::vector<unsigned char> encoded(meshopt_encodeVertexBufferBound(vertices.size(), sizeof(T)));
        encoded.resize(meshopt_encodeVertexBuffer(encoded.data(), encoded.size(), vertices.data(), vertices.size(), sizeof(T)));
        Add(type, ModelFile::Codec::MeshoptVertex, sizeof(T), uint32_t(vertices.size()), std::as_bytes(std::span(encoded)));
    }

    template <typename T>
    void AddIndices(std::span<const T> indices, size_t vertexCount)
    {
        std::vector<unsigned char> encoded(meshopt_encodeIndexBufferBound(indices.size(), vertexCount));
        encoded.resize(meshopt_encodeIndexBuffer(encoded.data(), encoded.size(), indices.data(), indices.size()));
        Add(ModelFile::SectionType::Indices, ModelFile::Codec::MeshoptIndex, sizeof(T), uint32_t(indices.size()), std::as_bytes(std::span(encoded)));
    }

    std::vector<std::byte> Finish(const ModelFile::Header& header)
    {
        assert(m_added == m_sectionCount && header.sectionCount == m_sectionCount);
        std::memcpy(m_bytes.data(), &header, sizeof(header));
        return std::move(m_bytes);
    }

private:
    static size_t AlignUp(size_t value, size_t alignment) { return (value + alignment - 1) / alignment * alignment; }

    uint16_t               m_sectionCount;
    uint16_t               m_added = 0;
    std::vector<std::byte> m_bytes;
};

std::expected<void, std::string> WriteModel(const std::filesystem::path& path, AssetId id, const ProcessedGeometry& geometry,
    std::span<const ModelFile::Material> materials)
{
    constexpr uint16_t kSectionCount = 5;

    ModelFileBuilder builder(kSectionCount);
    builder.AddRaw(ModelFile::SectionType::Submeshes, std::span(geometry.submeshes));
    builder.AddRaw(ModelFile::SectionType::Materials, materials);
    builder.AddVertices(ModelFile::SectionType::Positions, std::span(geometry.positions));
    builder.AddVertices(ModelFile::SectionType::Attributes, std::span(geometry.attributes));

    // Indices are relative to each submesh's base vertex, so 16 bits suffice whenever every
    // submesh has at most 65536 vertices.
    const bool shortIndices = std::ranges::all_of(geometry.submeshes, [](const ModelFile::Submesh& s) { return s.vertexCount <= 65536; });
    const size_t vertexCount = geometry.positions.size();
    if (shortIndices)
    {
        const std::vector<uint16_t> indices(geometry.indices.begin(), geometry.indices.end());
        builder.AddIndices(std::span(indices), vertexCount);
    }
    else
    {
        builder.AddIndices(std::span(geometry.indices), vertexCount);
    }

    ModelFile::Header header{};
    header.magic = ModelFile::kMagic;
    header.version = ModelFile::kVersion;
    header.sectionCount = kSectionCount;
    header.assetId = id;
    header.boundsMin[0] = geometry.boundsMin.x;
    header.boundsMin[1] = geometry.boundsMin.y;
    header.boundsMin[2] = geometry.boundsMin.z;
    header.boundsMax[0] = geometry.boundsMax.x;
    header.boundsMax[1] = geometry.boundsMax.y;
    header.boundsMax[2] = geometry.boundsMax.z;

    return WriteFileAtomic(path, builder.Finish(header));
}