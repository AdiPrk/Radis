#include <PCH/pch.h>
#include "ModelSerializer.h"
#include "Graphics/Common/Model.h"
#include "Graphics/Vulkan/VKMesh.h"
#include "Graphics/OpenGL/GLMesh.h"
#include "Graphics/RHI/IMesh.h"
#include "Graphics/Common/TextureLoader.h"
#include "BinaryIO.h"
#include "Engine.h"

using namespace Dog;

const std::string ModelSerializer::DOG_MODEL_FILE_PATH = "assets/models/nlm/";
const std::string ModelSerializer::DOG_MODEL_EXTENTION = ".nlm";

bool ModelSerializer::validateHeader(std::ifstream& file) {
    BinaryReaderLE reader(file);

    uint32_t hash = reader.U32(); // you ignore it, but still read
    uint32_t magic = reader.U32();
    uint32_t version = reader.U32();

    if (magic != MAGIC_NUMBER || version != VERSION)
    {
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

    BinaryWriterLE w(file);

    uint32_t magic = MAGIC_NUMBER;
    uint32_t version = VERSION;
    uint32_t hasAnim = model.mBoneInfoMap.empty() ? 0u : 1u;
    uint32_t checksum = 0; // reserved

    // Header: [hash][magic][version][hasAnim]
    w.U32(hash);
    w.U32(magic);
    w.U32(version);
    w.U32(hasAnim);
    // w.U32(checksum); // still unused

    // AABB
    w.Vec3(model.mAABBmin);
    w.Vec3(model.mAABBmax);

    // Mesh count
    uint32_t meshCount = static_cast<uint32_t>(model.mMeshes.size());
    w.U32(meshCount);

    auto WriteTexturePathEntry = [&](int32_t texId)
        {
            uint32_t embedded = 0;
            w.U32(embedded);

            if (texId < 0)
            {
                w.U32(0u); // nameSize = 0
                return;
            }

            const std::string& path = textures[static_cast<size_t>(texId)].outKTX2Path;
            w.String(path); // writes length + data
        };

    for (uint32_t index = 0; index < meshCount; ++index)
    {
        const auto& meshPtr = model.mMeshes[index];
        const auto& mesh = *meshPtr;
        const MeshTextureRefs& refs = meshTexRefs[index];

        uint32_t vertexCount = static_cast<uint32_t>(mesh.mVertices.size());
        uint32_t indexCount = static_cast<uint32_t>(mesh.mIndices.size());

        w.U32(vertexCount);
        w.U32(indexCount);

        // Vertices
        for (const auto& vertex : mesh.mVertices)
        {
            // If you want, you can pack this into a helper, but I'll keep it explicit for now
            w.Vec3(vertex.position);
            w.Vec3(vertex.color);
            w.Vec3(vertex.normal);
            w.Vec2(vertex.uv);

            // boneIDs and weights are already in memory in native endian,
            // but they are arrays of scalars so we still want endian-safe writes.
            // We can do them manually:
            for (int i = 0; i < 4; ++i)
                w.I32(vertex.boneIDs[i]);

            for (int i = 0; i < 4; ++i)
                w.F32(vertex.weights[i]);
        }

        // Indices (always via U32)
        for (uint32_t idxVal : mesh.mIndices)
            w.U32(idxVal);

        // Texture KTX2 paths
        WriteTexturePathEntry(refs.tex[0]);
        WriteTexturePathEntry(refs.tex[1]);
        WriteTexturePathEntry(refs.tex[2]);
        WriteTexturePathEntry(refs.tex[3]);
        WriteTexturePathEntry(refs.tex[4]);
        WriteTexturePathEntry(refs.tex[5]);

        w.U32(refs.metallicRoughnessCombined);
    }

    // Bones / animation
    if (!model.mBoneInfoMap.empty())
    {
        uint32_t boneCount = static_cast<uint32_t>(model.mBoneInfoMap.size());
        w.U32(boneCount);

        for (const auto& [boneName, boneInfo] : model.mBoneInfoMap)
        {
            w.String(boneName);
            w.I32(boneInfo.id);

            float rw = boneInfo.vqsOffset.rotation.w;
            float rx = boneInfo.vqsOffset.rotation.x;
            float ry = boneInfo.vqsOffset.rotation.y;
            float rz = boneInfo.vqsOffset.rotation.z;

            w.F32(rw);
            w.F32(rx);
            w.F32(ry);
            w.F32(rz);

            w.Vec3(boneInfo.vqsOffset.translation);
            w.Vec3(boneInfo.vqsOffset.scale);
        }
    }

    file.close();
}


bool ModelSerializer::load(Model& model, const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        DOG_CRITICAL("Could not open file for reading.");
        return false;
    }

    if (!validateHeader(file))
    {
        file.close();
        return false;
    }

    BinaryReaderLE r(file);

    uint32_t hasAnimation = r.U32();

    model.mMeshes.clear();

    // AABB
    model.mAABBmin = r.Vec3();
    model.mAABBmax = r.Vec3();

    uint32_t meshCount = r.U32();
    model.mMeshes.resize(meshCount);

    for (auto& meshPtr : model.mMeshes)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
            meshPtr = std::make_unique<VKMesh>();
        else
            meshPtr = std::make_unique<GLMesh>();

        auto& mesh = *meshPtr;

        uint32_t vertexCount = r.U32();
        uint32_t indexCount = r.U32();

        mesh.mVertices.resize(vertexCount);
        mesh.mIndices.resize(indexCount);

        // Vertices
        for (auto& vertex : mesh.mVertices)
        {
            vertex.position = r.Vec3();
            vertex.color = r.Vec3();
            vertex.normal = r.Vec3();
            vertex.uv = r.Vec2();

            for (int i = 0; i < 4; ++i)
                vertex.boneIDs[i] = r.I32();

            for (int i = 0; i < 4; ++i)
                vertex.weights[i] = r.F32();
        }

        // Indices
        for (uint32_t i = 0; i < indexCount; ++i)
            mesh.mIndices[i] = r.U32();

        // Textures
        auto ReadTextureData = [&](std::string& texturePath, std::vector<unsigned char>& textureData)
            {
                uint32_t embedded = r.U32();
                if (embedded)
                {
                    // legacy embedded
                    std::string texName = r.String();
                    uint32_t dataSize = r.U32();

                    textureData.resize(dataSize);
                    if (dataSize > 0)
                        file.read(reinterpret_cast<char*>(textureData.data()), dataSize);

                    texturePath.clear();
                }
                else
                {
                    std::string path = r.String();
                    texturePath = std::move(path);
                    textureData.clear();
                }
            };

        ReadTextureData(mesh.albedoTexturePath, mesh.mAlbedoTextureData);
        ReadTextureData(mesh.normalTexturePath, mesh.mNormalTextureData);
        ReadTextureData(mesh.metalnessTexturePath, mesh.mMetalnessTextureData);
        ReadTextureData(mesh.roughnessTexturePath, mesh.mRoughnessTextureData);
        ReadTextureData(mesh.occlusionTexturePath, mesh.mOcclusionTextureData);
        ReadTextureData(mesh.emissiveTexturePath, mesh.mEmissiveTextureData);

        uint32_t combined = r.U32();
        mesh.mMetallicRoughnessCombined = (combined != 0);
    }

    // Bones / animation
    model.mBoneInfoMap.clear();
    model.mBoneCount = 0;

    if (hasAnimation)
    {
        uint32_t boneCount = r.U32();
        int maxID = -1;
        model.mBoneInfoMap.reserve(boneCount);

        for (uint32_t i = 0; i < boneCount; ++i)
        {
            std::string boneName = r.String();
            int32_t boneID = r.I32();

            float rw = r.F32();
            float rx = r.F32();
            float ry = r.F32();
            float rz = r.F32();

            glm::vec3 t = r.Vec3();
            glm::vec3 s = r.Vec3();

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

