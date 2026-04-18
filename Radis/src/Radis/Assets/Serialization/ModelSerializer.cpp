/*****************************************************************//**
 * \file   ModelSerializer.cpp
 * \brief  Serializes and deserializes Models to/from disk
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "ModelSerializer.h"
#include "Graphics/Common/Model.h"
#include "Graphics/RHI/Mesh.h"
#include "Graphics/Common/TextureLoader.h"
#include "BinaryIO.h"
#include "Engine.h"

#include <meshoptimizer.h>
#include <mio/mio.hpp>

using namespace Radis;

const std::string ModelSerializer::RADIS_MODEL_FILE_PATH = "assets/models/dm/";
const std::string ModelSerializer::RADIS_MODEL_EXTENTION = ".dm";

bool ModelSerializer::validateHeader(BinaryReaderLE& reader)
{
    uint32_t hash = reader.U32(); // reserved, consumed but not validated
    uint32_t magic = reader.U32();
    uint32_t version = reader.U32();

    if (magic != MAGIC_NUMBER || version != VERSION)
    {
        RADIS_ERROR("Invalid file format or version.");
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
        Transmission,
        Count
    };

    static std::string TextureSlotNames[] =
    {
        "Albedo",
        "Normal",
        "Metalness",
        "Roughness",
        "Occlusion",
        "Emissive",
        "Transmission"
    };

    struct TextureRecord
    {
        std::string key;
        std::string sourcePath;
        const std::vector<unsigned char>* embeddedData = nullptr;
        std::string outKTX2Path;
    };

    struct MeshTextureRefs
    {
        int32_t tex[static_cast<size_t>(TextureSlot::Count)] = { -1, -1, -1, -1, -1, -1, -1 };
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
        std::string baseName = src.stem().string();
        std::string fileName = baseName + ".ktx2";
        return (modelDir / fileName).string();
    };

    auto MakeKTX2PathForEmbedded = [&](TextureSlot slot, std::size_t hashValue) -> std::string
    {
        std::string fileName = TextureSlotNames[static_cast<size_t>(slot)] + "_" + std::to_string(hashValue) + ".ktx2";
        return (modelDir / fileName).string();
    };

    std::unordered_map<std::string, uint32_t> texIdByKey;
    std::vector<TextureRecord> textures;
    std::vector<MeshTextureRefs> meshTexRefs;
    meshTexRefs.reserve(model.mMeshes.size());

    auto RegisterTexture = [&](const std::string& path, const std::vector<unsigned char>& data, TextureSlot slot) -> int32_t
    {
        if (path.empty() && data.empty())
            return -1;

        std::string key;
        std::string outPath;

        if (!path.empty())
        {
            key = "PATH|" + path;
            outPath = MakeKTX2PathForSource(path);
        }
        else
        {
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
        refs.tex[static_cast<size_t>(TextureSlot::Transmission)] = RegisterTexture(mesh.transmissionTexturePath, mesh.mTransmissionTextureData, TextureSlot::Transmission);

        refs.metallicRoughnessCombined = mesh.mMetallicRoughnessCombined ? 1u : 0u;
        meshTexRefs.push_back(refs);
    }

    std::for_each(std::execution::par, textures.begin(), textures.end(), [](TextureRecord& rec)
    {
        TextureLoader::KTX2BuildInput input{};
        input.sourcePath = rec.sourcePath;
        input.data = rec.embeddedData;
        TextureLoader::BuildKTX2File(input, rec.outKTX2Path);
    });

    // ---------------- FILE WRITE ---------------

    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        RADIS_CRITICAL("Could not open file for writing.");
        return;
    }

    BinaryWriterLE w(file);

    w.U32(hash);
    w.U32(MAGIC_NUMBER);
    w.U32(VERSION);
    w.U32(model.mBoneInfoMap.empty() ? 0u : 1u);

    w.Vec3(model.mAABBmin);
    w.Vec3(model.mAABBmax);

    uint32_t meshCount = static_cast<uint32_t>(model.mMeshes.size());
    w.U32(meshCount);

    auto WriteTexturePathEntry = [&](int32_t texId)
    {
        w.U32(0u); // embedded = false

        if (texId < 0)
        {
            w.U32(0u);
            return;
        }

        const std::string& path = textures[static_cast<size_t>(texId)].outKTX2Path;
        w.String(path);
    };

    for (uint32_t index = 0; index < meshCount; ++index)
    {
        const auto& mesh = *model.mMeshes[index];
        const MeshTextureRefs& refs = meshTexRefs[index];

        uint32_t vertexCount = static_cast<uint32_t>(mesh.mVertices.size());
        uint32_t indexCount = static_cast<uint32_t>(mesh.mIndices.size());
        size_t vertexStride = sizeof(decltype(mesh.mVertices)::value_type);
        size_t indexStride = sizeof(decltype(mesh.mIndices)::value_type);

        w.U32(vertexCount);
        w.U32(indexCount);

        // GPU Optimizations!
        std::vector<uint32_t> optIndicies(indexCount);
        meshopt_optimizeVertexCache(optIndicies.data(), mesh.mIndices.data(), indexCount, vertexCount);

        std::vector<decltype(mesh.mVertices)::value_type> optVerts(vertexCount);
        meshopt_optimizeVertexFetch(optVerts.data(), optIndicies.data(), indexCount, mesh.mVertices.data(), vertexCount, vertexStride);

        // Meshoptimizer Compression
        size_t maxIndexBound = meshopt_encodeIndexBufferBound(indexCount, vertexCount);
        std::vector<unsigned char> compIndicies(maxIndexBound);
        size_t compIndexSize = meshopt_encodeIndexBuffer(compIndicies.data(), compIndicies.size(), optIndicies.data(), indexCount);

        size_t maxVertexBound = meshopt_encodeVertexBufferBound(vertexCount, vertexStride);
        std::vector<unsigned char> compVerts(maxVertexBound);
        size_t compVertexSize = meshopt_encodeVertexBuffer(compVerts.data(), compVerts.size(), optVerts.data(), vertexCount, vertexStride);

        w.U32(static_cast<uint32_t>(compVertexSize));
        w.U32(static_cast<uint32_t>(compIndexSize));

        // Write Compressed Data
        w.PODArray(compVerts.data(), compVertexSize);
        w.PODArray(compIndicies.data(), compIndexSize);

        WriteTexturePathEntry(refs.tex[0]);
        WriteTexturePathEntry(refs.tex[1]);
        WriteTexturePathEntry(refs.tex[2]);
        WriteTexturePathEntry(refs.tex[3]);
        WriteTexturePathEntry(refs.tex[4]);
        WriteTexturePathEntry(refs.tex[5]);
        WriteTexturePathEntry(refs.tex[6]);

        w.U32(refs.metallicRoughnessCombined);
        w.Vec4(mesh.baseColorFactor);
        w.F32(mesh.metallicFactor);
        w.F32(mesh.roughnessFactor);
        w.Vec4(mesh.emissiveFactor);
        w.F32(mesh.transmissionFactor);
        w.F32(mesh.ior);
    }

    if (!model.mBoneInfoMap.empty())
    {
        w.U32(static_cast<uint32_t>(model.mBoneInfoMap.size()));

        for (const auto& [boneName, boneInfo] : model.mBoneInfoMap)
        {
            w.String(boneName);
            w.I32(boneInfo.id);
            w.F32(boneInfo.vqsOffset.rotation.w);
            w.F32(boneInfo.vqsOffset.rotation.x);
            w.F32(boneInfo.vqsOffset.rotation.y);
            w.F32(boneInfo.vqsOffset.rotation.z);
            w.Vec3(boneInfo.vqsOffset.translation);
            w.Vec3(boneInfo.vqsOffset.scale);
        }
    }

    file.close();
    // CompressInPlaceLZ4(filename); -> Completely Removed!
}


bool ModelSerializer::load(Model& model, const std::string& filename)
{
    // Memory-map the actual .dm file directly. No temp `.rawtmp` files anymore.
    std::error_code mmapError;
    mio::mmap_source mmap = mio::make_mmap_source(filename, mmapError);
    if (mmapError)
    {
        RADIS_CRITICAL("Failed to memory-map model file {}: {}", filename, mmapError.message());
        return false;
    }

    BinaryReaderLE r(mmap.data(), mmap.size());

    if (!validateHeader(r))
    {
        return false;
    }

    uint32_t hasAnimation = r.U32();

    model.mMeshes.clear();

    model.mAABBmin = r.Vec3();
    model.mAABBmax = r.Vec3();

    uint32_t meshCount = r.U32();
    model.mMeshes.resize(meshCount);

    for (auto& meshPtr : model.mMeshes)
    {
        meshPtr = std::make_unique<Mesh>();
        auto& mesh = *meshPtr;

        uint32_t vertexCount = r.U32();
        uint32_t indexCount = r.U32();
        uint32_t compVertexSize = r.U32();
        uint32_t compIndexSize = r.U32();

        size_t vertexStride = sizeof(decltype(mesh.mVertices)::value_type);
        size_t indexStride = sizeof(decltype(mesh.mIndices)::value_type);

        // 1. Read and Decode Vertices
        std::vector<unsigned char> compVerts;
        ResizeUninitialized(compVerts, compVertexSize);
        r.PODArray(compVerts.data(), compVertexSize);

        ResizeUninitialized(mesh.mVertices, vertexCount);
        int vertRes = meshopt_decodeVertexBuffer(mesh.mVertices.data(), vertexCount, vertexStride, compVerts.data(), compVertexSize);
        if (vertRes != 0) RADIS_ERROR("Failed to decode vertex buffer for mesh.");

        // 2. Read and Decode Indices
        std::vector<unsigned char> compIndices;
        ResizeUninitialized(compIndices, compIndexSize);
        r.PODArray(compIndices.data(), compIndexSize);

        ResizeUninitialized(mesh.mIndices, indexCount);
        int idxRes = meshopt_decodeIndexBuffer(mesh.mIndices.data(), indexCount, indexStride, compIndices.data(), compIndexSize);
        if (idxRes != 0) RADIS_ERROR("Failed to decode index buffer for mesh.");

        // Textures
        auto ReadTextureData = [&](std::string& texturePath, std::vector<unsigned char>& textureData)
        {
            uint32_t embedded = r.U32();
            if (embedded)
            {
                std::string texName = r.String();
                uint32_t dataSize = r.U32();

                textureData.resize(dataSize);
                if (dataSize > 0)
                    r.PODArray(textureData.data(), dataSize);

                texturePath.clear();
            }
            else
            {
                texturePath = r.String();
                textureData.clear();
            }
        };

        ReadTextureData(mesh.albedoTexturePath, mesh.mAlbedoTextureData);
        ReadTextureData(mesh.normalTexturePath, mesh.mNormalTextureData);
        ReadTextureData(mesh.metalnessTexturePath, mesh.mMetalnessTextureData);
        ReadTextureData(mesh.roughnessTexturePath, mesh.mRoughnessTextureData);
        ReadTextureData(mesh.occlusionTexturePath, mesh.mOcclusionTextureData);
        ReadTextureData(mesh.emissiveTexturePath, mesh.mEmissiveTextureData);
        ReadTextureData(mesh.transmissionTexturePath, mesh.mTransmissionTextureData);

        uint32_t combined = r.U32();
        mesh.mMetallicRoughnessCombined = (combined != 0);

        mesh.baseColorFactor = r.Vec4();
        mesh.metallicFactor = r.F32();
        mesh.roughnessFactor = r.F32();
        mesh.emissiveFactor = r.Vec4();
        mesh.transmissionFactor = r.F32();
        mesh.ior = r.F32();
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
            int32_t     boneID = r.I32();

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

    mmap.unmap();
    return true;
}