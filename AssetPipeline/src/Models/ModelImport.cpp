#include <pch.h>
#include "ModelImport.h"

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/type_ptr.hpp>

// State shared while converting one scene.
struct ImportContext
{
    const aiScene& scene;
    std::filesystem::path                            directory;       // the model's folder
    std::vector<std::filesystem::path>               textureFolders;  // see TextureFolders
    bool                                             fbx = false;
    ImportedScene& out;
    std::vector<int32_t>                             embeddedIndex;   // aiScene texture -> ImportedScene::embeddedImages, or -1
    std::unordered_map<std::string, ImportedTexture> images;          // by the reference written in the file
    std::map<aiTextureType, uint32_t>                ignoredSlots;    // texture slots no material slot reads, and how often
};

// Every texture slot ConvertMaterial reads. Unknown only counts for glTF, where it's metallicRoughness.
static constexpr aiTextureType kReadSlots[] =
{
    aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE, aiTextureType_OPACITY, aiTextureType_NORMALS,
    aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_METALNESS,
    aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE, aiTextureType_TRANSMISSION,
};

static glm::mat4 ToGlm(const aiMatrix4x4& m)
{
    return glm::transpose(glm::make_mat4(&m.a1));   // assimp stores rows, glm stores columns
}

// FBX files record their axis system in the scene metadata instead of converting it. Other formats
// have no such metadata and arrive in assimp's right-handed Y-up space, which is the engine's.
// Units are handled by aiProcess_GlobalScale, which applies the file's unit scale (FBX centimeters).
static glm::mat4 EngineSpaceConversion(const aiScene& scene, std::vector<std::string>& warnings)
{
    int32_t up = 1, upSign = 1, front = 2, frontSign = 1, coord = 0, coordSign = 1;
    if (const aiMetadata* metadata = scene.mMetaData)
    {
        metadata->Get("UpAxis", up);
        metadata->Get("UpAxisSign", upSign);
        metadata->Get("FrontAxis", front);
        metadata->Get("FrontAxisSign", frontSign);
        metadata->Get("CoordAxis", coord);
        metadata->Get("CoordAxisSign", coordSign);
    }

    const auto valid = [](int32_t axis) { return axis >= 0 && axis <= 2; };
    if (!valid(up) || !valid(front) || !valid(coord) || up == front || up == coord || front == coord)
    {
        warnings.push_back(std::format("ignoring invalid axis metadata (up {}, front {}, coord {})", up, front, coord));
        return glm::mat4(1.0f);
    }

    glm::vec3 right(0.0f), upAxis(0.0f), forward(0.0f);
    right[coord] = coordSign < 0 ? -1.0f : 1.0f;
    upAxis[up] = upSign < 0 ? -1.0f : 1.0f;
    forward[front] = frontSign < 0 ? -1.0f : 1.0f;

    // The rows are the file's right, up and front axes, which become the engine's X, Y and Z.
    return glm::mat4(glm::transpose(glm::mat3(right, upAxis, forward)));
}

// Copies the encoded images out of the scene. Raw pixel data (mHeight != 0) isn't supported yet.
static void CollectEmbeddedImages(ImportContext& ctx)
{
    for (uint32_t i = 0; i < ctx.scene.mNumTextures; ++i)
    {
        const aiTexture& texture = *ctx.scene.mTextures[i];
        if (texture.mHeight != 0)
        {
            ctx.out.warnings.push_back(std::format("embedded texture {} is raw pixel data, which isn't supported yet", i));
            ctx.embeddedIndex.push_back(-1);
            continue;
        }

        const auto* bytes = reinterpret_cast<const std::byte*>(texture.pcData);
        ctx.embeddedIndex.push_back(int32_t(ctx.out.embeddedImages.size()));
        ctx.out.embeddedImages.emplace_back(bytes, bytes + texture.mWidth);   // mWidth is the byte size
    }
}

// Where a model's texture files may be besides the path the model gives: next to the model, in a
// "textures" folder beside it, and in ModelTextures/<model name>/ in the model's folder or any
// folder above it, nearest first.
static std::vector<std::filesystem::path> TextureFolders(const std::filesystem::path& model)
{
    const std::filesystem::path        directory = model.parent_path();
    std::vector<std::filesystem::path> folders = { directory, directory / "textures" };

    std::error_code       ec;
    std::filesystem::path folder = std::filesystem::absolute(directory, ec);
    if (ec)
        return folders;

    for (;; folder = folder.parent_path())
    {
        if (const std::filesystem::path candidate = folder / "ModelTextures" / model.stem(); std::filesystem::is_directory(candidate, ec))
        {
            folders.push_back(candidate);
        }
        if (folder == folder.parent_path())
            break;
    }
    return folders;
}

// Finds the image a material refers to: an embedded one ("*0", or a matching file name), or a
// file. Exported paths are often absolute paths from the author's machine, so after the path as
// written, the file name is also tried in the model's texture folders (see TextureFolders).
static ImportedTexture ResolveImage(ImportContext& ctx, const std::string& reference)
{
    if (const auto found = ctx.images.find(reference); found != ctx.images.end())
    {
        return found->second;
    }

    ImportedTexture texture;
    if (const auto [embedded, index] = ctx.scene.GetEmbeddedTextureAndIndex(reference.c_str()); embedded)
    {
        texture.embedded = ctx.embeddedIndex[size_t(index)];
    }
    else
    {
        std::string normalized = reference;
        std::ranges::replace(normalized, '\\', '/');
        const std::filesystem::path written(std::u8string(normalized.begin(), normalized.end()));   // assimp strings are UTF-8

        std::vector<std::filesystem::path> candidates = { written.is_absolute() ? written : ctx.directory / written };
        for (const std::filesystem::path& folder : ctx.textureFolders)
        {
            candidates.push_back(folder / written.filename());
        }

        std::error_code ec;
        const auto existing = std::ranges::find_if(candidates, [&](const std::filesystem::path& p) { return std::filesystem::is_regular_file(p, ec); });
        if (existing != candidates.end())
        {
            texture.path = existing->lexically_normal();
        }
        else
        {
            ctx.out.warnings.push_back(std::format("texture not found: {}", reference));
        }
    }

    ctx.images.emplace(reference, texture);
    return texture;
}

static ModelFile::TextureWrap ToWrap(aiTextureMapMode mode)
{
    switch (mode)
    {
    case aiTextureMapMode_Clamp:
    case aiTextureMapMode_Decal:  return ModelFile::TextureWrap::ClampToEdge;
    case aiTextureMapMode_Mirror: return ModelFile::TextureWrap::MirroredRepeat;
    default:                      return ModelFile::TextureWrap::Repeat;
    }
}

// The first texture of `types` the material has. A texture that can't be found leaves the slot
// empty rather than falling through to the next type.
static ImportedTexture FindTexture(ImportContext& ctx, const aiMaterial& material, std::initializer_list<aiTextureType> types)
{
    for (const aiTextureType type : types)
    {
        aiString         reference;
        unsigned int     uvSet = 0;
        aiTextureMapMode modes[3] = { aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap };
        if (material.GetTexture(type, 0, &reference, nullptr, &uvSet, nullptr, nullptr, modes) != AI_SUCCESS)
            continue;

        if (uvSet != 0)
        {
            ctx.out.warnings.push_back(std::format("material {}: a texture uses UV set {}; only the first set is supported",
                material.GetName().C_Str(), uvSet));
        }

        ImportedTexture texture = ResolveImage(ctx, reference.C_Str());
        texture.wrapU = ToWrap(modes[0]);
        texture.wrapV = ToWrap(modes[1]);
        return texture;
    }
    return {};
}

static ImportedMaterial ConvertMaterial(ImportContext& ctx, const aiMaterial& material)
{
    ImportedMaterial out;
    out.name = material.GetName().C_Str();

    // Only assimp's glTF importer sets the alpha mode, so it doubles as the "this is glTF" check.
    aiString   alphaMode;
    const bool gltf = material.Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS;

    out.Texture(SourceSlot::BaseColor) = FindTexture(ctx, material, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE });
    out.Texture(SourceSlot::Opacity) = FindTexture(ctx, material, { aiTextureType_OPACITY });
    out.Texture(SourceSlot::Normal) = FindTexture(ctx, material, { aiTextureType_NORMALS });
    out.Texture(SourceSlot::Occlusion) = FindTexture(ctx, material, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP });   // glTF occlusion arrives as a lightmap
    out.Texture(SourceSlot::Roughness) = FindTexture(ctx, material, { aiTextureType_DIFFUSE_ROUGHNESS });
    out.Texture(SourceSlot::Metalness) = FindTexture(ctx, material, { aiTextureType_METALNESS });
    out.Texture(SourceSlot::Emissive) = FindTexture(ctx, material, { aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE });
    out.Texture(SourceSlot::Transmission) = FindTexture(ctx, material, { aiTextureType_TRANSMISSION });

    // Older assimp versions report glTF's metallicRoughness texture only under this key.
    if (gltf && !out.Texture(SourceSlot::Roughness) && !out.Texture(SourceSlot::Metalness))
    {
        const ImportedTexture metallicRoughness = FindTexture(ctx, material, { aiTextureType_UNKNOWN });
        out.Texture(SourceSlot::Roughness) = metallicRoughness;
        out.Texture(SourceSlot::Metalness) = metallicRoughness;
    }

    aiColor4D color;
    if (material.Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS)
    {
        out.baseColor = { color.r, color.g, color.b, color.a };
    }
    else if (material.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    {
        float opacity = 1.0f;
        material.Get(AI_MATKEY_OPACITY, opacity);
        out.baseColor = { color.r, color.g, color.b, opacity };
    }

    // In FBX a texture connected to the diffuse color replaces it, while exporters still write a
    // default color (Blender 0.8) or factor (Maya 0.8) next to it. glTF and OBJ multiply the two.
    if (ctx.fbx && out.Texture(SourceSlot::BaseColor))
    {
        out.baseColor = glm::vec4(1.0f, 1.0f, 1.0f, out.baseColor.a);
    }

    for (int type = aiTextureType_NONE + 1; type <= AI_TEXTURE_TYPE_MAX; ++type)
    {
        const bool read = std::ranges::contains(kReadSlots, aiTextureType(type)) || (gltf && type == aiTextureType_UNKNOWN);
        if (!read && material.GetTextureCount(aiTextureType(type)) > 0)
        {
            ++ctx.ignoredSlots[aiTextureType(type)];
        }
    }

    // Without a factor, a texture should show as authored rather than be multiplied away.
    if (material.Get(AI_MATKEY_METALLIC_FACTOR, out.metallic) != AI_SUCCESS)
    {
        out.metallic = out.Texture(SourceSlot::Metalness) ? 1.0f : 0.0f;
    }
    material.Get(AI_MATKEY_ROUGHNESS_FACTOR, out.roughness);

    aiColor3D emissive;
    if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
    {
        out.emissive = { emissive.r, emissive.g, emissive.b };
    }
    if (!gltf && out.Texture(SourceSlot::Emissive) && out.emissive == glm::vec3(0.0f))
    {
        out.emissive = glm::vec3(1.0f);   // other formats often leave the color black next to a texture
    }

    material.Get(AI_MATKEY_EMISSIVE_INTENSITY, out.emissiveStrength);
    material.Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), out.normalScale);
    if (material.Get(AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_LIGHTMAP, 0), out.occlusionStrength) != AI_SUCCESS)
    {
        material.Get(AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_AMBIENT_OCCLUSION, 0), out.occlusionStrength);
    }
    material.Get(AI_MATKEY_TRANSMISSION_FACTOR, out.transmission);
    material.Get(AI_MATKEY_REFRACTI, out.ior);

    int twoSided = 0;
    material.Get(AI_MATKEY_TWOSIDED, twoSided);
    out.doubleSided = twoSided != 0;

    if (gltf)
    {
        const std::string_view mode = alphaMode.C_Str();
        out.alphaMode = mode == "MASK" ? ModelFile::AlphaMode::Mask : mode == "BLEND" ? ModelFile::AlphaMode::Blend : ModelFile::AlphaMode::Opaque;
        material.Get(AI_MATKEY_GLTF_ALPHACUTOFF, out.alphaCutoff);
    }
    else if (out.Texture(SourceSlot::Opacity))
    {
        out.alphaMode = ModelFile::AlphaMode::Mask;   // separate opacity maps are almost always cutouts
    }
    else if (out.baseColor.a < 1.0f)
    {
        out.alphaMode = ModelFile::AlphaMode::Blend;
    }
    else if (ctx.fbx && out.Texture(SourceSlot::BaseColor))
    {
        out.alphaFromTexture = true;   // FBX uses the diffuse image's alpha channel as opacity when it has one
    }
    return out;
}

static void AddJoints(const aiNode& node, int32_t parent, const glm::mat4& conversion, const std::unordered_set<const aiNode*>& used,
    ImportedSkeleton& skeleton, std::unordered_map<const aiNode*, int32_t>& joints)
{
    if (!used.contains(&node))
        return;   // every ancestor of a used node is used, so nothing below this one is

    const int32_t  index = int32_t(skeleton.joints.size());
    ImportedJoint& joint = skeleton.joints.emplace_back();
    joint.name = node.mName.C_Str();
    joint.parent = parent;
    joint.local = parent < 0 ? conversion * ToGlm(node.mTransformation) : ToGlm(node.mTransformation);   // a root carries the conversion
    joint.global = parent < 0 ? joint.local : skeleton.joints[size_t(parent)].global * joint.local;
    joints.emplace(&node, index);

    for (uint32_t i = 0; i < node.mNumChildren; ++i)
    {
        AddJoints(*node.mChildren[i], index, conversion, used, skeleton, joints);
    }
}

// The skeleton is every node a bone names, plus their ancestors so that every joint's parent is a
// joint too. Returns the joint of each of those nodes.
static std::unordered_map<const aiNode*, int32_t> BuildSkeleton(const aiScene& scene, const glm::mat4& conversion, ImportedSkeleton& skeleton)
{
    std::unordered_set<const aiNode*> used;
    for (uint32_t m = 0; m < scene.mNumMeshes; ++m)
    {
        const aiMesh& mesh = *scene.mMeshes[m];
        for (uint32_t b = 0; b < mesh.mNumBones; ++b)
        {
            for (const aiNode* node = scene.mRootNode->FindNode(mesh.mBones[b]->mName); node && used.insert(node).second; node = node->mParent)
            {
            }
        }
    }

    std::unordered_map<const aiNode*, int32_t> joints;
    AddJoints(*scene.mRootNode, -1, conversion, used, skeleton, joints);
    return joints;
}

static std::optional<ImportedMesh> ConvertMesh(const aiMesh& mesh, const std::unordered_map<std::string, uint32_t>& jointByName, std::vector<std::string>& warnings)
{
    if (!(mesh.mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
    {
        warnings.push_back(std::format("mesh '{}' has no triangles and was skipped", mesh.mName.C_Str()));
        return std::nullopt;
    }

    ImportedMesh out;
    out.name = mesh.mName.C_Str();
    out.material = mesh.mMaterialIndex;

    const auto toVec3 = [](const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); };
    out.positions.resize(mesh.mNumVertices);
    std::transform(mesh.mVertices, mesh.mVertices + mesh.mNumVertices, out.positions.begin(), toVec3);

    if (mesh.HasNormals())
    {
        out.normals.resize(mesh.mNumVertices);
        std::transform(mesh.mNormals, mesh.mNormals + mesh.mNumVertices, out.normals.begin(), toVec3);
    }

    if (mesh.HasTextureCoords(0))
    {
        out.uvs.resize(mesh.mNumVertices);
        std::transform(mesh.mTextureCoords[0], mesh.mTextureCoords[0] + mesh.mNumVertices, out.uvs.begin(),
            [](const aiVector3D& uv) { return glm::vec2(uv.x, uv.y); });
    }

    if (mesh.HasVertexColors(0))
    {
        out.colors.resize(mesh.mNumVertices);
        std::transform(mesh.mColors[0], mesh.mColors[0] + mesh.mNumVertices, out.colors.begin(),
            [](const aiColor4D& c) { return glm::vec4(c.r, c.g, c.b, c.a); });
    }

    out.indices.reserve(size_t(mesh.mNumFaces) * 3);
    for (uint32_t i = 0; i < mesh.mNumFaces; ++i)
    {
        const aiFace& face = mesh.mFaces[i];
        if (face.mNumIndices == 3)
        {
            out.indices.insert(out.indices.end(), face.mIndices, face.mIndices + 3);
        }
    }

    if (out.indices.empty())
    {
        warnings.push_back(std::format("mesh '{}' has no triangles and was skipped", mesh.mName.C_Str()));
        return std::nullopt;
    }

    for (uint32_t i = 0; i < mesh.mNumBones; ++i)
    {
        const aiBone& bone = *mesh.mBones[i];
        const auto    joint = jointByName.find(bone.mName.C_Str());
        if (joint == jointByName.end())
        {
            warnings.push_back(std::format("mesh '{}': bone '{}' names no node, so its weights were dropped", out.name, bone.mName.C_Str()));
            continue;
        }

        ImportedBone& imported = out.bones.emplace_back();
        imported.joint = joint->second;
        imported.offset = ToGlm(bone.mOffsetMatrix);
        imported.weights.resize(bone.mNumWeights);
        std::transform(bone.mWeights, bone.mWeights + bone.mNumWeights, imported.weights.begin(),
            [](const aiVertexWeight& w) { return ImportedWeight{ w.mVertexId, w.mWeight }; });
    }
    return out;
}

static void AddNodes(const aiNode& node, int32_t parent, std::vector<ImportedNode>& nodes)
{
    const int32_t index = int32_t(nodes.size());
    nodes.push_back({ node.mName.C_Str(), parent, ToGlm(node.mTransformation) });
    for (uint32_t i = 0; i < node.mNumChildren; ++i)
    {
        AddNodes(*node.mChildren[i], index, nodes);
    }
}

template <typename Key, typename Value, typename Convert>
static std::vector<ImportedKey<Value>> ConvertKeys(const Key* keys, uint32_t count, double ticksPerSecond, Convert convert)
{
    std::vector<ImportedKey<Value>> out(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        out[i] = { float(keys[i].mTime / ticksPerSecond), convert(keys[i].mValue) };
    }
    return out;
}

// The rate the keys were authored at: the smallest gap between two keys of a channel.
static float EstimateSampleRate(const ImportedAnimation& animation)
{
    float gap = std::numeric_limits<float>::max();
    const auto measure = [&](const auto& keys)
        {
            for (size_t i = 1; i < keys.size(); ++i)
            {
                if (const float delta = keys[i].time - keys[i - 1].time; delta > 1e-4f)
                {
                    gap = std::min(gap, delta);
                }
            }
        };
    for (const ImportedChannel& channel : animation.channels)
    {
        measure(channel.translations);
        measure(channel.rotations);
        measure(channel.scales);
    }
    return gap == std::numeric_limits<float>::max() ? 30.0f : 1.0f / gap;
}

static ImportedAnimations ConvertAnimations(const aiScene& scene, const glm::mat4& conversion, std::vector<std::string>& warnings)
{
    ImportedAnimations out;
    out.conversion = conversion;
    AddNodes(*scene.mRootNode, -1, out.nodes);

    std::unordered_map<std::string, uint32_t> nodeByName;
    for (size_t i = 0; i < out.nodes.size(); ++i)
    {
        nodeByName.try_emplace(out.nodes[i].name, uint32_t(i));
    }

    const auto toVec3 = [](const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); };
    const auto toQuat = [](const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); };

    for (uint32_t a = 0; a < scene.mNumAnimations; ++a)
    {
        const aiAnimation& source = *scene.mAnimations[a];
        const double       ticksPerSecond = source.mTicksPerSecond > 0.0 ? source.mTicksPerSecond : 30.0;

        ImportedAnimation& animation = out.animations.emplace_back();
        animation.name = source.mName.C_Str();
        animation.duration = float(source.mDuration / ticksPerSecond);

        for (uint32_t c = 0; c < source.mNumChannels; ++c)
        {
            const aiNodeAnim& channel = *source.mChannels[c];
            const auto        node = nodeByName.find(channel.mNodeName.C_Str());
            if (node == nodeByName.end())
            {
                warnings.push_back(std::format("animation '{}' moves a node that doesn't exist: {}", animation.name, channel.mNodeName.C_Str()));
                continue;
            }

            ImportedChannel& imported = animation.channels.emplace_back();
            imported.node = node->second;
            imported.translations = ConvertKeys<aiVectorKey, glm::vec3>(channel.mPositionKeys, channel.mNumPositionKeys, ticksPerSecond, toVec3);
            imported.rotations = ConvertKeys<aiQuatKey, glm::quat>(channel.mRotationKeys, channel.mNumRotationKeys, ticksPerSecond, toQuat);
            imported.scales = ConvertKeys<aiVectorKey, glm::vec3>(channel.mScalingKeys, channel.mNumScalingKeys, ticksPerSecond, toVec3);
        }
        animation.sampleRate = EstimateSampleRate(animation);
    }
    return out;
}

static void CollectInstances(const aiNode& node, const glm::mat4& parent, int32_t joint, std::span<const int32_t> meshIndex,
    const std::unordered_map<const aiNode*, int32_t>& joints, ImportedScene& out)
{
    const glm::mat4 transform = parent * ToGlm(node.mTransformation);
    if (const auto found = joints.find(&node); found != joints.end())
    {
        joint = found->second;
    }

    for (uint32_t i = 0; i < node.mNumMeshes; ++i)
    {
        if (const int32_t mesh = meshIndex[node.mMeshes[i]]; mesh >= 0)
        {
            out.instances.push_back({ uint32_t(mesh), transform, joint });
        }
    }

    for (uint32_t i = 0; i < node.mNumChildren; ++i)
    {
        CollectInstances(*node.mChildren[i], transform, joint, meshIndex, joints, out);
    }
}

std::expected<ImportedScene, std::string> ImportModel(const std::filesystem::path& path)
{
    // Tangents, vertex cache order and node flattening are left to MeshProcessing, which does them
    // better; normals are only generated for meshes that have none.
    constexpr unsigned int kFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType
        | aiProcess_GenSmoothNormals | aiProcess_FindDegenerates | aiProcess_FindInvalidData | aiProcess_GenUVCoords
        | aiProcess_RemoveRedundantMaterials | aiProcess_GlobalScale | aiProcess_ValidateDataStructure;

    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
    importer.SetPropertyBool(AI_CONFIG_PP_FD_REMOVE, true);   // drop degenerate triangles instead of turning them into lines
    importer.SetPropertyBool(AI_CONFIG_PP_FD_CHECKAREA, false);   // keep small triangles; the area is measured before any scale
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);   // one node per FBX node instead of a chain of $AssimpFbx$ helpers

    const std::u8string utf8Path = path.u8string();          // assimp opens UTF-8 paths on every OS
    const aiScene* scene = importer.ReadFile(reinterpret_cast<const char*>(utf8Path.c_str()), kFlags);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        return std::unexpected(std::format("assimp: {}", importer.GetErrorString()));
    }

    ImportedScene out;
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    ImportContext ctx{ .scene = *scene, .directory = path.parent_path(), .textureFolders = TextureFolders(path), .fbx = extension == ".fbx", .out = out };
    CollectEmbeddedImages(ctx);

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        ImportedMaterial& material = out.materials.emplace_back(ConvertMaterial(ctx, *scene->mMaterials[i]));
        if (material.name.empty())
        {
            material.name = std::format("Material{}", i);   // texture names include the material's name
        }
    }

    if (!ctx.ignoredSlots.empty())
    {
        std::string slots;
        for (const auto& [type, count] : ctx.ignoredSlots)
        {
            slots += std::format("{}{} ({} materials)", slots.empty() ? "" : ", ", aiTextureTypeToString(type), count);
        }
        out.warnings.push_back("textures in slots the pipeline doesn't read were skipped: " + slots);
    }

    const glm::mat4 conversion = EngineSpaceConversion(*scene, out.warnings);
    const auto      joints = BuildSkeleton(*scene, conversion, out.skeleton);

    std::unordered_map<std::string, uint32_t> jointByName;
    for (size_t i = 0; i < out.skeleton.joints.size(); ++i)
    {
        jointByName.try_emplace(out.skeleton.joints[i].name, uint32_t(i));   // FindNode finds the first of a repeated name too
    }

    std::vector<int32_t> meshIndex(scene->mNumMeshes, -1);
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        if (auto mesh = ConvertMesh(*scene->mMeshes[i], jointByName, out.warnings))
        {
            meshIndex[i] = int32_t(out.meshes.size());
            out.meshes.push_back(std::move(*mesh));
        }
    }

    out.animations = ConvertAnimations(*scene, conversion, out.warnings);

    CollectInstances(*scene->mRootNode, conversion, -1, meshIndex, joints, out);

    std::vector<bool> placed(out.meshes.size());
    for (const ImportedInstance& instance : out.instances)
    {
        placed[instance.mesh] = true;
    }
    for (size_t i = 0; i < placed.size(); ++i)
    {
        if (!placed[i])
        {
            out.warnings.push_back(std::format("mesh '{}' isn't placed by any node and was skipped", out.meshes[i].name));
        }
    }
    if (out.instances.empty())
    {
        return std::unexpected("the file has no triangle meshes");
    }
    return out;
}

std::expected<ImportedAnimations, std::string> ImportAnimations(const std::filesystem::path& path, std::vector<std::string>& warnings)
{
    // The same scaling and pivot handling as ImportModel, so the nodes match the model's joints.
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

    // A file with only animations is flagged incomplete, which is fine here.
    const std::u8string utf8Path = path.u8string();
    const aiScene*      scene = importer.ReadFile(reinterpret_cast<const char*>(utf8Path.c_str()), aiProcess_GlobalScale);
    if (!scene || !scene->mRootNode)
    {
        return std::unexpected(std::format("assimp: {}", importer.GetErrorString()));
    }
    if (!scene->HasAnimations())
    {
        return std::unexpected("the file has no animations");
    }

    return ConvertAnimations(*scene, EngineSpaceConversion(*scene, warnings), warnings);
}
