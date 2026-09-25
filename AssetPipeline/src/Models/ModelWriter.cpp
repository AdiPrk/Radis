#include <pch.h>
#include "ModelWriter.h"
#include "SectionFileBuilder.h"
#include "Trs.h"
#include "../FileIO.h"

using ModelFileBuilder = SectionFileBuilder<ModelFile::Header, ModelFile::Section>;

// Null-terminated strings back to back; each distinct string is stored once.
class StringTable
{
public:
    uint32_t Add(const std::string& text)
    {
        const auto [it, inserted] = m_offsets.try_emplace(text, uint32_t(m_bytes.size()));
        if (inserted)
        {
            const auto* bytes = reinterpret_cast<const std::byte*>(text.c_str());
            m_bytes.insert(m_bytes.end(), bytes, bytes + text.size() + 1);
        }
        return it->second;
    }

    std::span<const std::byte> Bytes() const { return m_bytes; }

private:
    std::vector<std::byte>                    m_bytes;
    std::unordered_map<std::string, uint32_t> m_offsets;
};

static void StoreRestPose(const glm::mat4& m, ModelFile::Joint& joint)
{
    const Trs rest = Decompose(m);
    std::ranges::copy(std::array{ rest.translation.x, rest.translation.y, rest.translation.z }, joint.translation);
    std::ranges::copy(std::array{ rest.rotation.x, rest.rotation.y, rest.rotation.z, rest.rotation.w }, joint.rotation);
    std::ranges::copy(std::array{ rest.scale.x, rest.scale.y, rest.scale.z }, joint.scale);
}

static ModelFile::Matrix3x4 ToMatrix3x4(const glm::mat4& m)
{
    ModelFile::Matrix3x4 out{};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            out.rows[row][column] = m[column][row];
        }
    }
    return out;
}

std::expected<void, std::string> WriteModel(const std::filesystem::path& path, const ProcessedGeometry& geometry, std::span<const BuiltMaterial> materials,
    const ImportedSkeleton& skeleton)
{
    const bool     skinned = !skeleton.joints.empty();
    const uint16_t sectionCount = skinned ? 10 : 6;

    // Texture file names go into the string table; materials keep their offsets.
    StringTable                      strings;
    std::vector<ModelFile::Material> fileMaterials;
    for (const BuiltMaterial& built : materials)
    {
        ModelFile::Material& material = fileMaterials.emplace_back(built.material);
        for (size_t slot = 0; slot < ModelFile::kMaterialTextureCount; ++slot)
        {
            material.textures[slot] = built.textures[slot].empty() ? ModelFile::kNoTexture : strings.Add(built.textures[slot]);
        }
    }

    std::vector<ModelFile::Joint> joints;
    for (const ImportedJoint& imported : skeleton.joints)
    {
        ModelFile::Joint& joint = joints.emplace_back();
        joint.parent = int16_t(imported.parent);
        joint.name = strings.Add(imported.name);
        joint.nameHash = ModelFile::NameHash(imported.name.c_str());
        StoreRestPose(imported.local, joint);
    }

    std::vector<ModelFile::Matrix3x4> inverseBinds;
    std::ranges::transform(geometry.inverseBinds, std::back_inserter(inverseBinds), ToMatrix3x4);

    ModelFileBuilder builder(sectionCount);
    builder.AddRaw(ModelFile::SectionType::Submeshes, std::span(geometry.submeshes));
    builder.AddRaw(ModelFile::SectionType::Materials, std::span<const ModelFile::Material>(fileMaterials));
    builder.AddVertices(ModelFile::SectionType::Positions, std::span(geometry.positions));
    builder.AddVertices(ModelFile::SectionType::Attributes, std::span(geometry.attributes));

    // Indices are relative to each submesh's base vertex, so 16 bits suffice whenever every
    // submesh has at most 65536 vertices.
    const bool shortIndices = std::ranges::all_of(geometry.submeshes, [](const ModelFile::Submesh& s) { return s.vertexCount <= 65536; });
    const size_t vertexCount = geometry.positions.size();
    if (shortIndices)
    {
        const std::vector<uint16_t> indices(geometry.indices.begin(), geometry.indices.end());
        builder.AddIndices(ModelFile::SectionType::Indices, std::span(indices), vertexCount);
    }
    else
    {
        builder.AddIndices(ModelFile::SectionType::Indices, std::span(geometry.indices), vertexCount);
    }

    builder.AddRaw(ModelFile::SectionType::Strings, strings.Bytes());

    if (skinned)
    {
        builder.AddRaw(ModelFile::SectionType::Skeleton, std::span<const ModelFile::Joint>(joints));
        builder.AddRaw(ModelFile::SectionType::SkinJoints, std::span(geometry.paletteJoints));
        builder.AddRaw(ModelFile::SectionType::InverseBinds, std::span<const ModelFile::Matrix3x4>(inverseBinds));
        builder.AddVertices(ModelFile::SectionType::SkinWeights, std::span(geometry.skin));
    }

    ModelFile::Header header{};
    header.magic = ModelFile::kMagic;
    header.version = ModelFile::kVersion;
    header.sectionCount = sectionCount;
    header.boundsMin[0] = geometry.boundsMin.x;
    header.boundsMin[1] = geometry.boundsMin.y;
    header.boundsMin[2] = geometry.boundsMin.z;
    header.boundsMax[0] = geometry.boundsMax.x;
    header.boundsMax[1] = geometry.boundsMax.y;
    header.boundsMax[2] = geometry.boundsMax.z;

    return WriteFileAtomic(path, builder.Finish(header));
}