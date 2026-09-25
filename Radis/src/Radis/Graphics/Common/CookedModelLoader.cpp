/*****************************************************************//**
 * \file   CookedModelLoader.cpp
 * \brief  Loads models cooked by the asset pipeline.
 *********************************************************************/

#include <PCH/pch.h>
#include "CookedModelLoader.h"
#include "Graphics/RHI/Mesh.h"

#include <meshoptimizer.h>

namespace Radis
{
    namespace
    {
        using namespace ModelFile;

        constexpr const char* kSectionNames[] = { "Submeshes", "Materials", "Positions", "Attributes", "Indices", "Strings" };

        const char* SectionName(SectionType type)
        {
            return size_t(type) < std::size(kSectionNames) ? kSectionNames[size_t(type)] : "unknown";
        }

        // Decodes `section` into `out`, which must hold elementCount elements of elementSize bytes.
        bool DecodeInto(const Section& section, const std::byte* data, void* out, const std::string& path)
        {
            const size_t decodedSize = size_t(section.elementSize) * section.elementCount;
            const auto* encoded = reinterpret_cast<const unsigned char*>(data);

            int result = 0;
            switch (section.codec)
            {
            case Codec::None:
                if (section.size != decodedSize)
                {
                    RADIS_ERROR("{}: {} section holds {} bytes where {} are needed", path, SectionName(section.type), section.size, decodedSize);
                    return false;
                }
                if (decodedSize > 0)   // an empty section (a model without textures has no strings) may have no buffer
                {
                    std::memcpy(out, data, decodedSize);
                }
                break;

            case Codec::MeshoptVertex:
                result = meshopt_decodeVertexBuffer(out, section.elementCount, section.elementSize, encoded, size_t(section.size));
                break;

            case Codec::MeshoptIndex:
                result = meshopt_decodeIndexBuffer(out, section.elementCount, section.elementSize, encoded, size_t(section.size));
                break;

            default:
                RADIS_ERROR("{}: {} section uses an unknown codec", path, SectionName(section.type));
                return false;
            }

            if (result != 0)
            {
                RADIS_ERROR("{}: decoding the {} section failed ({})", path, SectionName(section.type), result);
                return false;
            }
            return true;
        }

        template <typename T>
        bool DecodeArray(const Section& section, const std::byte* data, std::vector<T>& out, const std::string& path)
        {
            if (section.elementSize != sizeof(T))
            {
                RADIS_ERROR("{}: {} section has {}-byte elements, expected {}", path, SectionName(section.type), section.elementSize, sizeof(T));
                return false;
            }

            out.resize(section.elementCount);
            return DecodeInto(section, data, out.data(), path);
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

        bool ReadFile(const std::string& path, std::vector<std::byte>& out)
        {
            std::ifstream         file(path, std::ios::binary | std::ios::ate);
            const std::streamsize size = file ? std::streamsize(file.tellg()) : -1;
            if (size < 0)
            {
                RADIS_ERROR("{}: cannot open", path);
                return false;
            }

            out.resize(static_cast<size_t>(size));
            file.seekg(0);
            if (!file.read(reinterpret_cast<char*>(out.data()), size))
            {
                RADIS_ERROR("{}: cannot read", path);
                return false;
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
        if (!ReadFile(path, file))
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
            default:                      continue;   // sections this engine doesn't know about are skipped
            }

            if (!decoded)
            {
                return false;
            }
            found[size_t(section.type)] = true;
        }

        for (size_t i = 0; i < std::size(found); ++i)
        {
            if (!found[i])
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
}