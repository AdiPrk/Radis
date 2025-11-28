#include <PCH/pch.h>
#include "ModelSerializer.h"
#include "Graphics/Common/Model.h"
#include "Graphics/Vulkan/VKMesh.h"
#include "Graphics/OpenGL/GLMesh.h"
#include "Graphics/RHI/IMesh.h"
#include "Graphics/Common/TextureLoader.h"
#include "Engine.h"

using namespace Dog::VFS;

const std::string ModelSerializer::DOG_MODEL_FILE_PATH = "assets/models/nlm/";
const std::string ModelSerializer::DOG_MODEL_EXTENTION = ".nlm";

constexpr uint32_t ModelSerializer::swapEndian(uint32_t val) {
    return ((val >> 24) & 0xff)
        | ((val << 8) & 0xff0000)
        | ((val >> 8) & 0xff00)
        | ((val << 24) & 0xff000000);
}

constexpr uint64_t ModelSerializer::swapEndian64(uint64_t val) {
    return ((val >> 56) & 0x00000000000000FFULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 56) & 0xFF00000000000000ULL);
}

// Utility to swap endianness of a float
float ModelSerializer::swapEndianFloat(float val) {
    if constexpr (switchEndian) {
        uint32_t temp;
        std::memcpy(&temp, &val, sizeof(float));
        temp = swapEndian(temp);
        std::memcpy(&val, &temp, sizeof(float));
    }
    return val;
}

// Utility to return a glm::vec2 with swapped endianness
glm::vec2 ModelSerializer::swapEndianVec2(const glm::vec2& vec) {
    return glm::vec2(swapEndianFloat(vec.x), swapEndianFloat(vec.y));
}

// Utility to return a glm::vec3 with swapped endianness
glm::vec3 ModelSerializer::swapEndianVec3(const glm::vec3& vec) {
    return glm::vec3(swapEndianFloat(vec.x), swapEndianFloat(vec.y), swapEndianFloat(vec.z));
}

// Utility to return a glm::mat4 with swapped endianness
glm::mat4 ModelSerializer::swapEndianMat4(const glm::mat4& mat) {
    glm::mat4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i][j] = swapEndianFloat(mat[i][j]);
        }
    }
    return result;
}

bool ModelSerializer::validateHeader(std::ifstream& file) {
    uint32_t hash, magic, version;
    file.read(reinterpret_cast<char*>(&hash), sizeof(hash));
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    // Swap endianness if the system is big-endian
    if (switchEndian) {
        magic = swapEndian(magic);
        version = swapEndian(version);
    }

    if (magic != MAGIC_NUMBER || version != VERSION) {
        DOG_ERROR("Invalid file format or version.");
        return false;
    }

    return true;
}

void ModelSerializer::save(const Model& model, const std::string& filename, uint32_t hash)
{
    // ---------------- TEXTURE REGISTRY / DEDUP ---------------------------

    enum class TextureSlot : uint8_t
    {
        Albedo = 0,
        Normal,
        Metalness,
        Roughness,
        Occlusion,
        Emissive,
        Count
    };

    static std::string TextureSlotNames[] =
    {
        "Albedo",
        "Normal",
        "Metalness",
        "Roughness",
        "Occlusion",
        "Emissive"
    };

    struct TextureRecord
    {
        std::string key;                // dedup key
        std::string sourcePath;        // original (non-ktx2) path if any
        const std::vector<unsigned char>* embeddedData = nullptr; // if no path
        std::string outKTX2Path;       // final KTX2 path we will write to disk
    };

    struct MeshTextureRefs
    {
        int32_t tex[static_cast<size_t>(TextureSlot::Count)] = { -1, -1, -1, -1, -1, -1 };
        uint32_t metallicRoughnessCombined = 0;
    };

    auto HashBytes = [](const std::vector<unsigned char>& data) -> std::size_t
    {
        if (data.empty()) return 0;
        return std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
    };

    // KTX2 root: <ModelsPath>/ktx2/<ModelName>/
    std::filesystem::path ktxRoot = std::filesystem::path(Assets::ModelsPath) / "ktx2";
    std::filesystem::path modelDir = ktxRoot / model.mModelName;

    auto MakeKTX2PathForSource = [&](const std::string& srcPath) -> std::string
    {
        std::filesystem::path src(srcPath);
        std::string baseName = src.stem().string();  // original filename without extension
        std::string fileName = baseName + ".ktx2";

        std::filesystem::path full = modelDir / fileName;
        return full.string();
    };

    auto MakeKTX2PathForEmbedded = [&](TextureSlot slot, std::size_t hashValue) -> std::string
    {
        std::string fileName = TextureSlotNames[static_cast<size_t>(slot)] + "_" + std::to_string(hashValue) + ".ktx2";

        std::filesystem::path full = modelDir / fileName;
        return full.string();
    };

    std::unordered_map<std::string, uint32_t> texIdByKey;
    std::vector<TextureRecord> textures;
    std::vector<MeshTextureRefs> meshTexRefs;
    meshTexRefs.reserve(model.mMeshes.size());

    auto RegisterTexture = [&](const std::string& path, const std::vector<unsigned char>& data, TextureSlot slot) -> int32_t
    {
        if (path.empty() && data.empty())
            return -1; // no texture

        std::string key;
        std::string outPath;

        if (!path.empty())
        {
            // Dedup key: distinguish path-based textures by path string
            key = "PATH|" + path;
            outPath = MakeKTX2PathForSource(path);
        }
        else
        {
            // Dedup by content hash for embedded textures
            size_t h = HashBytes(data);
            key = "EMBEDDED|" + std::to_string(h);
            outPath = MakeKTX2PathForEmbedded(slot, h);
        }

        auto it = texIdByKey.find(key);
        if (it != texIdByKey.end())
            return static_cast<int32_t>(it->second);

        uint32_t newId = static_cast<uint32_t>(textures.size());
        texIdByKey.emplace(key, newId);

        TextureRecord rec;
        rec.key = std::move(key);
        rec.sourcePath = path;
        rec.embeddedData = path.empty() ? &data : nullptr;
        rec.outKTX2Path = std::move(outPath);

        textures.push_back(std::move(rec));
        return static_cast<int32_t>(newId);
    };

    // First pass: build registry + per-mesh refs (dedup within this model)
    for (const auto& meshPtr : model.mMeshes)
    {
        const auto& mesh = *meshPtr;
        MeshTextureRefs refs{};

        refs.tex[static_cast<size_t>(TextureSlot::Albedo)] = RegisterTexture(mesh.albedoTexturePath, mesh.mAlbedoTextureData, TextureSlot::Albedo);
        refs.tex[static_cast<size_t>(TextureSlot::Normal)] = RegisterTexture(mesh.normalTexturePath, mesh.mNormalTextureData, TextureSlot::Normal);
        refs.tex[static_cast<size_t>(TextureSlot::Metalness)] = RegisterTexture(mesh.metalnessTexturePath, mesh.mMetalnessTextureData, TextureSlot::Metalness);
        refs.tex[static_cast<size_t>(TextureSlot::Roughness)] = RegisterTexture(mesh.roughnessTexturePath, mesh.mRoughnessTextureData, TextureSlot::Roughness);
        refs.tex[static_cast<size_t>(TextureSlot::Occlusion)] = RegisterTexture(mesh.occlusionTexturePath, mesh.mOcclusionTextureData, TextureSlot::Occlusion);
        refs.tex[static_cast<size_t>(TextureSlot::Emissive)] = RegisterTexture(mesh.emissiveTexturePath, mesh.mEmissiveTextureData, TextureSlot::Emissive);

        refs.metallicRoughnessCombined = mesh.mMetallicRoughnessCombined ? 1u : 0u;

        meshTexRefs.push_back(refs);
    }

    // ---------------- PARALLEL KTX2 BUILD ---------------------------
    std::for_each(std::execution::par, textures.begin(), textures.end(), [](TextureRecord& rec)
    {
        TextureLoader::KTX2BuildInput input{};
        input.sourcePath = rec.sourcePath;
        input.data = rec.embeddedData;

        TextureLoader::BuildKTX2File(input, rec.outKTX2Path);
    });

    // ---------------- FILE WRITE (paths to KTX2s only) ---------------

    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        DOG_CRITICAL("Could not open file for writing.");
        return;
    }

    auto WriteU32 = [&](uint32_t value)
        {
            if constexpr (switchEndian) value = swapEndian(value);
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };

    auto WriteI32 = [&](int32_t value)
        {
            if constexpr (switchEndian) value = static_cast<int32_t>(swapEndian(static_cast<uint32_t>(value)));
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };

    auto WriteU64 = [&](uint64_t value)
        {
            if constexpr (switchEndian) value = swapEndian64(value);
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };

    uint32_t magic = MAGIC_NUMBER;
    uint32_t version = VERSION;
    uint32_t checksum = 0; // reserved
    uint32_t hasAnimation = model.mBoneInfoMap.empty() ? 0u : 1u;

    // Header
    WriteU32(hash);
    WriteU32(magic);
    WriteU32(version);
    WriteU32(hasAnimation);
    // WriteU32(checksum); // still unused

    // AABB
    if constexpr (switchEndian)
    {
        glm::vec3 min = swapEndianVec3(model.mAABBmin);
        glm::vec3 max = swapEndianVec3(model.mAABBmax);
        file.write(reinterpret_cast<const char*>(&min), sizeof(min));
        file.write(reinterpret_cast<const char*>(&max), sizeof(max));
    }
    else
    {
        file.write(reinterpret_cast<const char*>(&model.mAABBmin), sizeof(model.mAABBmin));
        file.write(reinterpret_cast<const char*>(&model.mAABBmax), sizeof(model.mAABBmax));
    }

    // Mesh count
    uint32_t meshCount = static_cast<uint32_t>(model.mMeshes.size());
    WriteU32(meshCount);

    auto WriteTexturePathEntry = [&](int32_t texId)
        {
            // Same layout as your original path-based version:
            // embedded=0, nameSize + path string (or nameSize=0 for "no texture")
            uint32_t embedded = 0;
            WriteU32(embedded);

            if (texId < 0)
            {
                uint32_t nameSize = 0;
                WriteU32(nameSize);
                return;
            }

            const std::string& path = textures[static_cast<size_t>(texId)].outKTX2Path;
            uint32_t nameSize = static_cast<uint32_t>(path.size());
            WriteU32(nameSize);
            if (nameSize > 0)
            {
                file.write(path.c_str(), nameSize);
            }
        };

    // Each mesh
    for (uint32_t index = 0; index < meshCount; ++index)
    {
        const auto& meshPtr = model.mMeshes[index];
        const auto& mesh = *meshPtr;
        const MeshTextureRefs& refs = meshTexRefs[index];

        uint32_t vertexCount = static_cast<uint32_t>(mesh.mVertices.size());
        uint32_t indexCount = static_cast<uint32_t>(mesh.mIndices.size());

        WriteU32(vertexCount);
        WriteU32(indexCount);

        // Vertices
        for (const auto& vertex : mesh.mVertices)
        {
            if constexpr (switchEndian)
            {
                glm::vec3 position = swapEndianVec3(vertex.position);
                glm::vec3 color = swapEndianVec3(vertex.color);
                glm::vec3 normal = swapEndianVec3(vertex.normal);
                glm::vec2 uv = swapEndianVec2(vertex.uv);

                glm::ivec4 boneIDs(
                    static_cast<int32_t>(swapEndian(static_cast<uint32_t>(vertex.boneIDs[0]))),
                    static_cast<int32_t>(swapEndian(static_cast<uint32_t>(vertex.boneIDs[1]))),
                    static_cast<int32_t>(swapEndian(static_cast<uint32_t>(vertex.boneIDs[2]))),
                    static_cast<int32_t>(swapEndian(static_cast<uint32_t>(vertex.boneIDs[3])))
                );

                glm::vec4 weights(
                    swapEndianFloat(vertex.weights[0]),
                    swapEndianFloat(vertex.weights[1]),
                    swapEndianFloat(vertex.weights[2]),
                    swapEndianFloat(vertex.weights[3])
                );

                file.write(reinterpret_cast<const char*>(&position), sizeof(position));
                file.write(reinterpret_cast<const char*>(&color), sizeof(color));
                file.write(reinterpret_cast<const char*>(&normal), sizeof(normal));
                file.write(reinterpret_cast<const char*>(&uv), sizeof(uv));
                file.write(reinterpret_cast<const char*>(&boneIDs), sizeof(boneIDs));
                file.write(reinterpret_cast<const char*>(&weights), sizeof(weights));
            }
            else
            {
                file.write(reinterpret_cast<const char*>(&vertex.position), sizeof(vertex.position));
                file.write(reinterpret_cast<const char*>(&vertex.color), sizeof(vertex.color));
                file.write(reinterpret_cast<const char*>(&vertex.normal), sizeof(vertex.normal));
                file.write(reinterpret_cast<const char*>(&vertex.uv), sizeof(vertex.uv));
                file.write(reinterpret_cast<const char*>(vertex.boneIDs.data()), sizeof(vertex.boneIDs));
                file.write(reinterpret_cast<const char*>(vertex.weights.data()), sizeof(vertex.weights));
            }
        }

        // Indices
        for (uint32_t idxVal : mesh.mIndices)
        {
            WriteU32(idxVal);
        }

        // Texture entries: now always paths to KTX2 files in
        // Assets::ModelsPath/ktx2/<ModelName>/
        WriteTexturePathEntry(refs.tex[static_cast<size_t>(TextureSlot::Albedo)]);
        WriteTexturePathEntry(refs.tex[static_cast<size_t>(TextureSlot::Normal)]);
        WriteTexturePathEntry(refs.tex[static_cast<size_t>(TextureSlot::Metalness)]);
        WriteTexturePathEntry(refs.tex[static_cast<size_t>(TextureSlot::Roughness)]);
        WriteTexturePathEntry(refs.tex[static_cast<size_t>(TextureSlot::Occlusion)]);
        WriteTexturePathEntry(refs.tex[static_cast<size_t>(TextureSlot::Emissive)]);

        WriteU32(refs.metallicRoughnessCombined);
    }

    // Bones / animation as before
    if (!model.mBoneInfoMap.empty())
    {
        uint32_t boneCount = static_cast<uint32_t>(model.mBoneInfoMap.size());
        WriteU32(boneCount);

        for (const auto& [boneName, boneInfo] : model.mBoneInfoMap)
        {
            uint32_t nameSize = static_cast<uint32_t>(boneName.size());
            WriteU32(nameSize);
            file.write(boneName.c_str(), nameSize);

            WriteI32(boneInfo.id);

            float rw = boneInfo.vqsOffset.rotation.w;
            float rx = boneInfo.vqsOffset.rotation.x;
            float ry = boneInfo.vqsOffset.rotation.y;
            float rz = boneInfo.vqsOffset.rotation.z;
            glm::vec3 t = boneInfo.vqsOffset.translation;
            glm::vec3 s = boneInfo.vqsOffset.scale;

            if constexpr (switchEndian)
            {
                rw = swapEndianFloat(rw);
                rx = swapEndianFloat(rx);
                ry = swapEndianFloat(ry);
                rz = swapEndianFloat(rz);
                t = swapEndianVec3(t);
                s = swapEndianVec3(s);
            }

            file.write(reinterpret_cast<const char*>(&rw), sizeof(float));
            file.write(reinterpret_cast<const char*>(&rx), sizeof(float));
            file.write(reinterpret_cast<const char*>(&ry), sizeof(float));
            file.write(reinterpret_cast<const char*>(&rz), sizeof(float));
            file.write(reinterpret_cast<const char*>(&t), sizeof(glm::vec3));
            file.write(reinterpret_cast<const char*>(&s), sizeof(glm::vec3));
        }
    }

    file.close();
}


bool ModelSerializer::load(Model& model, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        DOG_CRITICAL("Could not open file for reading.");
        return false;
    }

    // Validate the file header and version
    if (!validateHeader(file))
    {
        file.close();
        return false;
    }

    uint32_t hasAnimation;
    file.read(reinterpret_cast<char*>(&hasAnimation), sizeof(hasAnimation));
    if constexpr (switchEndian) hasAnimation = swapEndian(hasAnimation);

    // Clear meshes
    model.mMeshes.clear();

    // Read AABB
    file.read(reinterpret_cast<char*>(&model.mAABBmin), sizeof(model.mAABBmin));
    file.read(reinterpret_cast<char*>(&model.mAABBmax), sizeof(model.mAABBmax));

    if constexpr (switchEndian) {
        model.mAABBmin = swapEndianVec3(model.mAABBmin);
        model.mAABBmax = swapEndianVec3(model.mAABBmax);
    }

    // Read mesh count
    uint32_t meshCount;
    file.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));
    if constexpr (switchEndian) meshCount = swapEndian(meshCount);

    model.mMeshes.resize(meshCount);

    // Read each mesh
    for (auto& mesh : model.mMeshes)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            mesh = std::make_unique<VKMesh>();
        }
        else
        {
            mesh = std::make_unique<GLMesh>();
        }

        uint32_t vertexCount, indexCount;

        // Read vertex and index counts
        file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        if constexpr (switchEndian) {
            vertexCount = swapEndian(vertexCount);
            indexCount = swapEndian(indexCount);
        }

        mesh->mVertices.resize(vertexCount);
        mesh->mIndices.resize(indexCount);

        // Read vertices
        for (auto& vertex : mesh->mVertices) {
            file.read(reinterpret_cast<char*>(&vertex.position), sizeof(vertex.position));
            file.read(reinterpret_cast<char*>(&vertex.color), sizeof(vertex.color));
            file.read(reinterpret_cast<char*>(&vertex.normal), sizeof(vertex.normal));
            file.read(reinterpret_cast<char*>(&vertex.uv), sizeof(vertex.uv));
            file.read(reinterpret_cast<char*>(vertex.boneIDs.data()), sizeof(vertex.boneIDs));
            file.read(reinterpret_cast<char*>(vertex.weights.data()), sizeof(vertex.weights));

            if constexpr (switchEndian) {
                vertex.position = swapEndianVec3(vertex.position);
                vertex.color = swapEndianVec3(vertex.color);
                vertex.normal = swapEndianVec3(vertex.normal);
                vertex.uv = swapEndianVec2(vertex.uv);
                for (auto& id : vertex.boneIDs) {
                    id = static_cast<int>(swapEndian(static_cast<uint32_t>(id)));
                }
                for (auto& weight : vertex.weights) {
                    weight = swapEndianFloat(weight);
                }
            }
        }

        // Read indices
        file.read(reinterpret_cast<char*>(mesh->mIndices.data()), indexCount * sizeof(uint32_t));

        // Swap endianness of indices if necessary
        if constexpr (switchEndian) {
            for (auto& index : mesh->mIndices) {
                index = swapEndian(index);
            }
        }

        // Read texture data (for new files: this only reads KTX2 paths)
        auto ReadTextureData = [&](std::string& texturePath, std::vector<unsigned char>& textureData)
            {
                uint32_t embedded;
                file.read(reinterpret_cast<char*>(&embedded), sizeof(embedded));
                if constexpr (switchEndian) embedded = swapEndian(embedded);

                if (embedded)
                {
                    // Legacy embedded texture path + raw bytes
                    uint32_t nameSize;
                    file.read(reinterpret_cast<char*>(&nameSize), sizeof(nameSize));
                    if constexpr (switchEndian) nameSize = swapEndian(nameSize);

                    std::string texName(nameSize, '\0');
                    file.read(&texName[0], nameSize);

                    uint32_t dataSize;
                    file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
                    if constexpr (switchEndian) dataSize = swapEndian(dataSize);

                    textureData.resize(dataSize);
                    file.read(reinterpret_cast<char*>(textureData.data()), dataSize);

                    texturePath.clear(); // Indicates embedded data
                }
                else
                {
                    // Path-only case (new format: this will be path to a .ktx2 file)
                    uint32_t nameSize;
                    file.read(reinterpret_cast<char*>(&nameSize), sizeof(nameSize));
                    if constexpr (switchEndian) nameSize = swapEndian(nameSize);

                    texturePath.resize(nameSize);
                    if (nameSize > 0)
                        file.read(&texturePath[0], nameSize);

                    textureData.clear(); // indicates texture from file
                }
            };

        ReadTextureData(mesh->albedoTexturePath, mesh->mAlbedoTextureData);
        ReadTextureData(mesh->normalTexturePath, mesh->mNormalTextureData);
        ReadTextureData(mesh->metalnessTexturePath, mesh->mMetalnessTextureData);
        ReadTextureData(mesh->roughnessTexturePath, mesh->mRoughnessTextureData);
        ReadTextureData(mesh->occlusionTexturePath, mesh->mOcclusionTextureData);
        ReadTextureData(mesh->emissiveTexturePath, mesh->mEmissiveTextureData);

        uint32_t combined;
        file.read(reinterpret_cast<char*>(&combined), sizeof(combined));
        if constexpr (switchEndian) combined = swapEndian(combined);
        mesh->mMetallicRoughnessCombined = (combined != 0);
    }

    // Bones / animation (unchanged)
    model.mBoneInfoMap.clear();
    model.mBoneCount = 0;

    if (hasAnimation)
    {
        uint32_t boneCount = 0;
        file.read(reinterpret_cast<char*>(&boneCount), sizeof(boneCount));
        if constexpr (switchEndian) boneCount = swapEndian(boneCount);

        int maxID = -1;
        model.mBoneInfoMap.reserve(boneCount);

        for (uint32_t i = 0; i < boneCount; ++i)
        {
            uint32_t nameSize = 0;
            file.read(reinterpret_cast<char*>(&nameSize), sizeof(nameSize));
            if constexpr (switchEndian) nameSize = swapEndian(nameSize);

            std::string boneName(nameSize, '\0');
            if (nameSize > 0)
            {
                file.read(&boneName[0], nameSize);
            }

            int32_t boneID = 0;
            file.read(reinterpret_cast<char*>(&boneID), sizeof(boneID));
            if constexpr (switchEndian) {
                boneID = static_cast<int32_t>(swapEndian(static_cast<uint32_t>(boneID)));
            }

            float rw, rx, ry, rz;
            glm::vec3 t, s;

            file.read(reinterpret_cast<char*>(&rw), sizeof(float));
            file.read(reinterpret_cast<char*>(&rx), sizeof(float));
            file.read(reinterpret_cast<char*>(&ry), sizeof(float));
            file.read(reinterpret_cast<char*>(&rz), sizeof(float));
            file.read(reinterpret_cast<char*>(&t), sizeof(glm::vec3));
            file.read(reinterpret_cast<char*>(&s), sizeof(glm::vec3));

            if constexpr (switchEndian) {
                rw = swapEndianFloat(rw);
                rx = swapEndianFloat(rx);
                ry = swapEndianFloat(ry);
                rz = swapEndianFloat(rz);
                t = swapEndianVec3(t);
                s = swapEndianVec3(s);
            }

            VQS vqs;
            vqs.rotation = glm::quat(rw, rx, ry, rz);
            vqs.translation = t;
            vqs.scale = s;

            model.mBoneInfoMap.emplace(boneName, BoneInfo(boneID, vqs));
            if (boneID > maxID) maxID = boneID;
        }

        model.mBoneCount = (maxID >= 0) ? (maxID + 1) : 0;
    }

    file.close();
    return true;
}
