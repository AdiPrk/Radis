#pragma once

#include "ModelFormat.h"

#include <meshoptimizer.h>

// Builds a file in the layout of ModelFormat.h (a header, a section table, then the sections'
// data) in memory; AnimationFormat.h uses the same layout. The header and section table are filled
// in as sections are added.
template <typename Header, typename Section>
class SectionFileBuilder
{
public:
    using SectionType = decltype(Section::type);

    explicit SectionFileBuilder(uint16_t sectionCount)
        : m_sectionCount(sectionCount)
        , m_bytes(sizeof(Header) + sizeof(Section) * sectionCount)
    {
    }

    void Add(SectionType type, ModelFile::Codec codec, uint32_t elementSize, uint32_t elementCount, std::span<const std::byte> data)
    {
        assert(m_added < m_sectionCount);
        m_bytes.resize(AlignUp(m_bytes.size(), ModelFile::kSectionAlignment));

        const Section section{ type, codec, elementSize, elementCount, m_bytes.size(), data.size() };
        std::memcpy(m_bytes.data() + sizeof(Header) + sizeof(Section) * m_added++, &section, sizeof(section));
        m_bytes.insert(m_bytes.end(), data.begin(), data.end());
    }

    template <typename T>
    void AddRaw(SectionType type, std::span<const T> elements)
    {
        Add(type, ModelFile::Codec::None, sizeof(T), uint32_t(elements.size()), std::as_bytes(elements));
    }

    template <typename T>
    void AddVertices(SectionType type, std::span<const T> vertices)
    {
        std::vector<unsigned char> encoded(meshopt_encodeVertexBufferBound(vertices.size(), sizeof(T)));
        encoded.resize(meshopt_encodeVertexBuffer(encoded.data(), encoded.size(), vertices.data(), vertices.size(), sizeof(T)));
        Add(type, ModelFile::Codec::MeshoptVertex, sizeof(T), uint32_t(vertices.size()), std::as_bytes(std::span(encoded)));
    }

    template <typename T>
    void AddIndices(SectionType type, std::span<const T> indices, size_t vertexCount)
    {
        std::vector<unsigned char> encoded(meshopt_encodeIndexBufferBound(indices.size(), vertexCount));
        encoded.resize(meshopt_encodeIndexBuffer(encoded.data(), encoded.size(), indices.data(), indices.size()));
        Add(type, ModelFile::Codec::MeshoptIndex, sizeof(T), uint32_t(indices.size()), std::as_bytes(std::span(encoded)));
    }

    std::vector<std::byte> Finish(const Header& header)
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
