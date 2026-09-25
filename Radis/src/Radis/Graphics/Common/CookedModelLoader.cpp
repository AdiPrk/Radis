/*****************************************************************//**
 * \file   CookedModelLoader.cpp
 * \brief  Loads models cooked by the asset pipeline.
 *********************************************************************/

#include <PCH/pch.h>
#include "CookedModelLoader.h"
#include "AnimationFormat.h"
#include "CookedFile.h"
#include "Animation/Skeleton.h"
#include "Graphics/RHI/Mesh.h"

namespace Radis
{
    namespace
    {
        using namespace ModelFile;

        constexpr const char* kSectionNames[] = { "Submeshes", "Materials", "Positions", "Attributes", "Indices", "Strings",
                                                  "Skeleton", "SkinJoints", "InverseBinds", "SkinWeights" };
        constexpr size_t      kRequiredSections = 6;   // the skin sections are only in skinned models

        const char* SectionName(SectionType type)
        {
            return size_t(type) < std::size(kSectionNames) ? kSectionNames[size_t(type)] : "unknown";
        }

        template <typename T>
        bool DecodeArray(const Section& section, const std::byte* data, std::vector<T>& out, const std::string& path)
        {
            return CookedFile::DecodeArray(section, data, out, path, SectionName(section.type));
        }

        bool DecodeIndices(const Section& section, const std::byte* data, std::vector<uint32_t>& out, const std::string& path)
        {
            if (section.elementSize == sizeof(uint32_t))
            {
                return DecodeArray(section, data, out, path);
            }

            std::vector<uint16_t> narrow;
            if (!DecodeArray(section, data, narrow, path))
            {
                return false;
            }
            out.assign(narrow.begin(), narrow.end());
            return true;
        }

        // Joints come after their parents and have names; the palette names joints; every skinned
        // vertex names palette entries.
        bool ValidateSkin(const CookedModelData& data, const std::string& path, const auto& validName)
        {
            for (size_t i = 0; i < data.joints.size(); ++i)
            {
                const Joint& joint = data.joints[i];
                if (joint.parent < -1 || joint.parent >= int32_t(i) || joint.name == kNoTexture || !validName(joint.name))
                {
                    RADIS_ERROR("{}: joint {} has a bad parent or name", path, i);
                    return false;
                }
            }

            if (data.inverseBinds.size() != data.skinJoints.size()
                || std::any_of(data.skinJoints.begin(), data.skinJoints.end(), [&](uint16_t joint) { return joint >= data.joints.size(); }))
            {
                RADIS_ERROR("{}: the skin palette doesn't match the skeleton", path);
                return false;
            }

            if (data.skin.size() != data.positions.size())
            {
                RADIS_ERROR("{}: {} positions but {} skin weights", path, data.positions.size(), data.skin.size());
                return false;
            }

            for (const SkinWeights& s : data.skin)
            {
                for (int k = 0; k < 4; ++k)
                {
                    if (s.weights[k] > 0 && s.joints[k] >= data.skinJoints.size())
                    {
                        RADIS_ERROR("{}: a vertex names a palette entry past the palette", path);
                        return false;
                    }
                }
            }
            return true;
        }

        // Every submesh must stay inside the buffers and reference valid materials and vertices, and
        // every texture name must be a string inside the table.
        bool Validate(const CookedModelData& data, const std::string& path)
        {
            const auto validName = [&](uint32_t offset)
                {
                    return offset == kNoTexture
                        || (offset < data.strings.size() && std::find(data.strings.begin() + offset, data.strings.end(), '\0') != data.strings.end());
                };
            if (!data.joints.empty() && !ValidateSkin(data, path, validName))
            {
                return false;
            }
            for (size_t i = 0; i < data.materials.size(); ++i)
            {
                if (!std::all_of(std::begin(data.materials[i].textures), std::end(data.materials[i].textures), validName))
                {
                    RADIS_ERROR("{}: material {} names a texture outside the string table", path, i);
                    return false;
                }
            }

            if (data.positions.size() != data.attributes.size())
            {
                RADIS_ERROR("{}: {} positions but {} vertex attributes", path, data.positions.size(), data.attributes.size());
                return false;
            }

            for (size_t i = 0; i < data.submeshes.size(); ++i)
            {
                const Submesh& s = data.submeshes[i];
                if (s.material >= data.materials.size()
                    || size_t(s.firstIndex) + s.indexCount > data.indices.size()
                    || size_t(s.baseVertex) + s.vertexCount > data.positions.size()
                    || s.indexCount % 3 != 0)
                {
                    RADIS_ERROR("{}: submesh {} refers outside the model", path, i);
                    return false;
                }

                const auto first = data.indices.begin() + s.firstIndex;
                if (std::any_of(first, first + s.indexCount, [&](uint32_t index) { return index >= s.vertexCount; }))
                {
                    RADIS_ERROR("{}: submesh {} has an index past its vertices", path, i);
                    return false;
                }
            }
            return true;
        }
    }

    bool CookedModelLoader::Load(const std::string& path, CookedModelData& out)
    {
        std::vector<std::byte> file;
        if (!CookedFile::ReadFile(path, file))
        {
            return false;
        }

        if (file.size() < sizeof(Header))
        {
            RADIS_ERROR("{}: too small to be a cooked model", path);
            return false;
        }

        std::memcpy(&out.header, file.data(), sizeof(Header));
        if (out.header.magic != kMagic)
        {
            RADIS_ERROR("{}: not a cooked model", path);
            return false;
        }
        if (out.header.version != kVersion)
        {
            RADIS_ERROR("{}: cooked with format version {}, the engine reads version {}; re-cook it", path, out.header.version, kVersion);
            return false;
        }

        const size_t tableEnd = sizeof(Header) + sizeof(Section) * out.header.sectionCount;
        if (file.size() < tableEnd)
        {
            RADIS_ERROR("{}: the section table is truncated", path);
            return false;
        }

        std::vector<Section> sections(out.header.sectionCount);
        std::memcpy(sections.data(), file.data() + sizeof(Header), sizeof(Section) * sections.size());

        bool found[std::size(kSectionNames)] = {};
        for (const Section& section : sections)
        {
            if (section.offset > file.size() || section.size > file.size() - section.offset)
            {
                RADIS_ERROR("{}: the {} section lies outside the file", path, SectionName(section.type));
                return false;
            }

            const std::byte* data = file.data() + section.offset;
            bool             decoded = false;
            switch (section.type)
            {
            case SectionType::Submeshes:  decoded = DecodeArray(section, data, out.submeshes, path);  break;
            case SectionType::Materials:  decoded = DecodeArray(section, data, out.materials, path);  break;
            case SectionType::Positions:  decoded = DecodeArray(section, data, out.positions, path);  break;
            case SectionType::Attributes: decoded = DecodeArray(section, data, out.attributes, path); break;
            case SectionType::Indices:    decoded = DecodeIndices(section, data, out.indices, path);  break;
            case SectionType::Strings:    decoded = DecodeArray(section, data, out.strings, path);    break;
            case SectionType::Skeleton:     decoded = DecodeArray(section, data, out.joints, path);       break;
            case SectionType::SkinJoints:   decoded = DecodeArray(section, data, out.skinJoints, path);   break;
            case SectionType::InverseBinds: decoded = DecodeArray(section, data, out.inverseBinds, path); break;
            case SectionType::SkinWeights:  decoded = DecodeArray(section, data, out.skin, path);         break;
            default:                      continue;   // sections this engine doesn't know about are skipped
            }

            if (!decoded)
            {
                return false;
            }
            found[size_t(section.type)] = true;
        }

        const bool skinned = std::any_of(std::begin(found) + kRequiredSections, std::end(found), [](bool f) { return f; });
        for (size_t i = 0; i < std::size(found); ++i)
        {
            if (!found[i] && (i < kRequiredSections || skinned))
            {
                RADIS_ERROR("{}: the {} section is missing", path, kSectionNames[i]);
                return false;
            }
        }
        return Validate(out, path);
    }

    std::vector<std::unique_ptr<Mesh>> CookedModelLoader::CreateMeshes(const CookedModelData& data, const std::filesystem::path& textureDirectory)
    {
        const auto texturePath = [&](uint32_t offset) -> std::string
        {
            const char* name = data.TextureName(offset);
            return name ? (textureDirectory / std::u8string_view(reinterpret_cast<const char8_t*>(name))).string() : std::string();
        };

        std::vector<std::unique_ptr<Mesh>> meshes;
        meshes.reserve(data.submeshes.size());

        for (const Submesh& submesh : data.submeshes)
        {
            Mesh& mesh = *meshes.emplace_back(std::make_unique<Mesh>());

            // Indices are relative to the submesh's first vertex, which is exactly how a Mesh
            // indexes its own vertex array, so they copy straight across.
            const auto firstIndex = data.indices.begin() + submesh.firstIndex;
            mesh.mIndices.assign(firstIndex, firstIndex + submesh.indexCount);

            mesh.mVertices.resize(submesh.vertexCount);
            for (uint32_t i = 0; i < submesh.vertexCount; ++i)
            {
                const Position& p = data.positions[submesh.baseVertex + i];
                const VertexAttributes& a = data.attributes[submesh.baseVertex + i];

                Vertex& vertex = mesh.mVertices[i];
                vertex.position = glm::vec3(p.xyz[0], p.xyz[1], p.xyz[2]);
                vertex.normal = glm::vec3(a.normal[0], a.normal[1], a.normal[2]);
                vertex.uv = glm::vec2(a.uv[0], a.uv[1]);
                vertex.color = glm::vec3(a.color[0], a.color[1], a.color[2]);
                vertex.tangent = glm::vec4(a.tangent[0], a.tangent[1], a.tangent[2], a.tangent[3]);

                if (!data.skin.empty())
                {
                    const SkinWeights& s = data.skin[submesh.baseVertex + i];
                    for (int k = 0; k < Vertex::MAX_BONE_INFLUENCE; ++k)
                    {
                        vertex.boneIDs[k] = s.weights[k] > 0 ? int(s.joints[k]) : -1;
                        vertex.weights[k] = float(s.weights[k]) / 65535.0f;
                    }
                }
            }

            const Material& m = data.materials[submesh.material];
            mesh.baseColorFactor = glm::vec4(m.baseColor[0], m.baseColor[1], m.baseColor[2], m.baseColor[3]);
            mesh.metallicFactor = m.metallic;
            mesh.roughnessFactor = m.roughness;
            mesh.emissiveFactor = glm::vec4(glm::vec3(m.emissive[0], m.emissive[1], m.emissive[2]) * m.emissiveStrength, 0.0f);
            mesh.transmissionFactor = m.transmission;
            mesh.ior = m.ior;

            mesh.albedoTexturePath = texturePath(m.textures[size_t(MaterialTexture::BaseColor)]);
            mesh.normalTexturePath = texturePath(m.textures[size_t(MaterialTexture::Normal)]);
            mesh.emissiveTexturePath = texturePath(m.textures[size_t(MaterialTexture::Emissive)]);
            mesh.transmissionTexturePath = texturePath(m.textures[size_t(MaterialTexture::Transmission)]);

            // ORM is glTF's layout: occlusion in R, roughness in G, metalness in B. One texture feeds
            // all three slots (the shaders read .r, .g and .b); TextureLibrary loads it once.
            const std::string orm = texturePath(m.textures[size_t(MaterialTexture::ORM)]);
            mesh.metalnessTexturePath = orm;
            mesh.roughnessTexturePath = orm;
            mesh.occlusionTexturePath = orm;
            mesh.mMetallicRoughnessCombined = !orm.empty();
        }
        return meshes;
    }

    std::unique_ptr<Skeleton> CookedModelLoader::CreateSkeleton(const CookedModelData& data)
    {
        if (data.joints.empty())
        {
            return nullptr;
        }

        auto skeleton = std::make_unique<Skeleton>();
        for (size_t i = 0; i < data.joints.size(); ++i)
        {
            const Joint& joint = data.joints[i];
            skeleton->parents.push_back(joint.parent);
            skeleton->names.emplace_back(data.JointName(i));
            skeleton->restPose.push_back({
                .translation = glm::vec3(joint.translation[0], joint.translation[1], joint.translation[2]),
                .rotation = glm::quat(joint.rotation[3], joint.rotation[0], joint.rotation[1], joint.rotation[2]),
                .scale = glm::vec3(joint.scale[0], joint.scale[1], joint.scale[2]),
                });
        }

        skeleton->paletteJoints = data.skinJoints;
        for (const Matrix3x4& rows : data.inverseBinds)
        {
            glm::mat4 inverseBind(1.0f);
            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 4; ++column)
                {
                    inverseBind[column][row] = rows.rows[row][column];
                }
            }
            skeleton->inverseBinds.push_back(inverseBind);
        }

        skeleton->hash = AnimationFile::SkeletonHash(data.joints.data(), data.joints.size());
        return skeleton;
    }
}
