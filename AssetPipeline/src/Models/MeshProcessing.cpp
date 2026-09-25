#include <pch.h>
#include "MeshProcessing.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <meshoptimizer.h>
#include <MikkTSpace/mikktspace.h>

// Working vertex while processing; every field is a float, so there is no padding for the
// byte-wise comparisons of welding to trip over.
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent{ 1.0f, 0.0f, 0.0f, 1.0f };
    glm::vec2 uv{ 0.0f };
    glm::vec4 color{ 1.0f };
    glm::vec4 joints{ 0.0f };    // palette entries, most significant first
    glm::vec4 weights{ 0.0f };   // sum to 1, or all 0 for a vertex that isn't skinned
};
static_assert(sizeof(Vertex) == 96);

// Palette entries: a joint and the inverse bind matrix that goes with it. Meshes bound to a joint
// with the same matrix share an entry.
class Palette
{
public:
    uint32_t Add(uint32_t joint, const glm::mat4& inverseBind)
    {
        for (size_t i = 0; i < m_joints.size(); ++i)
        {
            if (m_joints[i] == joint && Near(m_inverseBinds[i], inverseBind))
                return uint32_t(i);
        }

        m_joints.push_back(uint16_t(joint));
        m_inverseBinds.push_back(inverseBind);
        return uint32_t(m_joints.size() - 1);
    }

    std::vector<uint16_t>&  Joints() { return m_joints; }
    std::vector<glm::mat4>& InverseBinds() { return m_inverseBinds; }

private:
    static bool Near(const glm::mat4& a, const glm::mat4& b)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 3; ++row)
            {
                if (std::abs(a[column][row] - b[column][row]) > 1e-4f * std::max(1.0f, std::abs(a[column][row])))
                    return false;
            }
        }
        return true;
    }

    std::vector<uint16_t>  m_joints;
    std::vector<glm::mat4> m_inverseBinds;
};

// A vertex's four largest influences, most significant first and normalized. Returns the share of
// the vertex's weight that didn't fit.
static float KeepLargestInfluences(std::span<const std::pair<uint32_t, float>> influences, Vertex& v)
{
    std::array<std::pair<uint32_t, float>, 4> kept{};
    float                                     total = 0.0f;
    for (const auto& influence : influences)
    {
        total += influence.second;
        const auto smallest = std::ranges::min_element(kept, {}, &std::pair<uint32_t, float>::second);
        if (influence.second > smallest->second)
        {
            *smallest = influence;
        }
    }
    std::ranges::sort(kept, std::greater{}, &std::pair<uint32_t, float>::second);

    const float sum = kept[0].second + kept[1].second + kept[2].second + kept[3].second;
    if (sum <= 0.0f)
        return 0.0f;

    for (int i = 0; i < 4; ++i)
    {
        v.joints[i] = float(kept[i].first);
        v.weights[i] = kept[i].second / sum;
    }
    return 1.0f - sum / total;
}

// Unorm16 weights that sum to exactly 65535; the rounding error goes to the largest.
static ModelFile::SkinWeights QuantizeSkin(const Vertex& v)
{
    ModelFile::SkinWeights out{};
    int                    total = 0;
    for (int i = 0; i < 4; ++i)
    {
        out.joints[i] = uint16_t(v.joints[i]);
        out.weights[i] = uint16_t(std::lround(v.weights[i] * 65535.0f));
        total += out.weights[i];
    }
    if (total > 0)
    {
        out.weights[0] = uint16_t(out.weights[0] + 65535 - total);
    }
    return out;
}

static void Store(const glm::vec3& v, float(&out)[3])
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

static glm::vec3 SafeNormalize(const glm::vec3& v)
{
    const float length = glm::length(v);
    return length > 1e-12f ? v / length : glm::vec3(0.0f, 1.0f, 0.0f);   // also taken for NaN
}

// Where an instance's vertices are baked. A skinned mesh goes where its joints' rest pose puts it:
// in FBX that's where its node puts it too, but glTF ignores the node of a skinned mesh. The bone
// with the most weight decides, for a file whose rest pose isn't the pose it was bound in.
static glm::mat4 BakeTransform(const ImportedScene& scene, const ImportedMesh& mesh, const ImportedInstance& instance)
{
    if (mesh.bones.empty() || scene.skeleton.joints.empty())
        return instance.transform;

    const auto totalWeight = [](const ImportedBone& bone)
        {
            float total = 0.0f;
            for (const ImportedWeight& w : bone.weights)
            {
                total += w.weight;
            }
            return total;
        };
    const ImportedBone& heaviest = *std::ranges::max_element(mesh.bones, {}, totalWeight);
    return scene.skeleton.joints[heaviest.joint].global * heaviest.offset;
}

// Binds the vertices of a skinned model's instance to palette entries. A bone's offset maps from
// the mesh's own space, so it's rebased onto the baked vertices by the inverse of `transform`.
// Returns the largest share of a vertex's weight that was dropped.
static float BindInstance(const ImportedScene& scene, const ImportedMesh& mesh, const ImportedInstance& instance, const glm::mat4& transform, Palette& palette,
    std::vector<Vertex>& vertices)
{
    if (mesh.bones.empty())
    {
        if (instance.joint < 0)
            return 0.0f;

        const uint32_t entry = palette.Add(uint32_t(instance.joint), glm::inverse(scene.skeleton.joints[size_t(instance.joint)].global));
        for (Vertex& v : vertices)
        {
            v.joints = glm::vec4(float(entry), 0.0f, 0.0f, 0.0f);
            v.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        }
        return 0.0f;
    }

    const glm::mat4 toMesh = glm::inverse(transform);
    std::vector<std::vector<std::pair<uint32_t, float>>> influences(vertices.size());
    for (const ImportedBone& bone : mesh.bones)
    {
        const uint32_t entry = palette.Add(bone.joint, bone.offset * toMesh);
        for (const ImportedWeight& w : bone.weights)
        {
            if (w.vertex < vertices.size() && w.weight > 0.0f)
            {
                influences[w.vertex].emplace_back(entry, w.weight);
            }
        }
    }

    float dropped = 0.0f;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        dropped = std::max(dropped, KeepLargestInfluences(influences[i], vertices[i]));
    }
    return dropped;
}

// Appends the instance's triangles as unshared corners (3 per triangle) in engine space.
static float AppendInstance(const ImportedScene& scene, const ImportedMesh& mesh, const ImportedInstance& instance, Palette& palette, std::vector<Vertex>& corners)
{
    const glm::mat4 transform = BakeTransform(scene, mesh, instance);
    const glm::mat3 linear(transform);
    const glm::mat3 normalMatrix = glm::inverseTranspose(linear);

    std::vector<Vertex> vertices(mesh.positions.size());
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        Vertex& v = vertices[i];
        v.position = glm::vec3(transform * glm::vec4(mesh.positions[i], 1.0f));
        v.normal = SafeNormalize(normalMatrix * (mesh.normals.empty() ? glm::vec3(0.0f) : mesh.normals[i]));
        if (!mesh.uvs.empty()) v.uv = mesh.uvs[i];
        if (!mesh.colors.empty()) v.color = mesh.colors[i];
    }

    const float dropped = scene.skeleton.joints.empty() ? 0.0f : BindInstance(scene, mesh, instance, transform, palette, vertices);

    // A mirroring transform turns the triangles inside out; swapping two corners restores the winding.
    const bool mirrored = glm::determinant(linear) < 0.0f;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        corners.push_back(vertices[mesh.indices[i]]);
        corners.push_back(vertices[mesh.indices[i + (mirrored ? 2 : 1)]]);
        corners.push_back(vertices[mesh.indices[i + (mirrored ? 1 : 2)]]);
    }
    return dropped;
}

static Vertex& Corner(const SMikkTSpaceContext* context, int face, int vertex)
{
    return (*static_cast<std::vector<Vertex>*>(context->m_pUserData))[size_t(face) * 3 + size_t(vertex)];
}

// MikkTSpace, the tangent space normal maps are baked against. It runs on the bottom-left UVs, so
// the bitangent points up the image, the +Y of the normal maps.
static void GenerateTangents(std::vector<Vertex>& corners)
{
    SMikkTSpaceInterface callbacks{};
    callbacks.m_getNumFaces = [](const SMikkTSpaceContext* context)
        {
            return int(static_cast<const std::vector<Vertex>*>(context->m_pUserData)->size() / 3);
        };
    callbacks.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) { return 3; };
    callbacks.m_getPosition = [](const SMikkTSpaceContext* context, float out[], int face, int vertex)
        {
            std::memcpy(out, &Corner(context, face, vertex).position, sizeof(glm::vec3));
        };
    callbacks.m_getNormal = [](const SMikkTSpaceContext* context, float out[], int face, int vertex)
        {
            std::memcpy(out, &Corner(context, face, vertex).normal, sizeof(glm::vec3));
        };
    callbacks.m_getTexCoord = [](const SMikkTSpaceContext* context, float out[], int face, int vertex)
        {
            std::memcpy(out, &Corner(context, face, vertex).uv, sizeof(glm::vec2));
        };
    callbacks.m_setTSpaceBasic = [](const SMikkTSpaceContext* context, const float tangent[], float sign, int face, int vertex)
        {
            Corner(context, face, vertex).tangent = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
        };

    SMikkTSpaceContext context{ &callbacks, &corners };
    genTangSpaceDefault(&context);
}

// Shares identical corners, then orders triangles for the vertex cache and overdraw and vertices
// for fetch locality. Returns the vertices; `indices` receives the triangle list.
static std::vector<Vertex> WeldAndOptimize(const std::vector<Vertex>& corners, std::vector<uint32_t>& indices)
{
    std::vector<uint32_t> remap(corners.size());
    const size_t uniqueCount = meshopt_generateVertexRemap(remap.data(), nullptr, corners.size(), corners.data(), corners.size(), sizeof(Vertex));

    std::vector<Vertex> vertices(uniqueCount);
    meshopt_remapVertexBuffer(vertices.data(), corners.data(), corners.size(), sizeof(Vertex), remap.data());
    indices.resize(corners.size());
    meshopt_remapIndexBuffer(indices.data(), nullptr, corners.size(), remap.data());

    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
    meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &vertices[0].position.x, vertices.size(), sizeof(Vertex), 1.05f);
    vertices.resize(meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Vertex)));
    return vertices;
}

ProcessedGeometry ProcessMeshes(const ImportedScene& scene)
{
    // Instances grouped by material; std::map keeps the submesh order stable.
    std::map<uint32_t, std::vector<const ImportedInstance*>> byMaterial;
    for (const ImportedInstance& instance : scene.instances)
    {
        byMaterial[scene.meshes[instance.mesh].material].push_back(&instance);
    }

    ProcessedGeometry out;
    out.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    out.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

    const bool skinned = !scene.skeleton.joints.empty();
    Palette    palette;

    for (const auto& [material, instances] : byMaterial)
    {
        std::vector<Vertex> corners;
        for (const ImportedInstance* instance : instances)
        {
            out.droppedWeight = std::max(out.droppedWeight, AppendInstance(scene, scene.meshes[instance->mesh], *instance, palette, corners));
        }
        if (corners.empty())
            continue;

        if (material < scene.materials.size() && scene.materials[material].Texture(SourceSlot::Normal))
        {
            GenerateTangents(corners);
        }

        std::vector<uint32_t>     indices;
        const std::vector<Vertex> vertices = WeldAndOptimize(corners, indices);

        ModelFile::Submesh submesh{};
        submesh.firstIndex = uint32_t(out.indices.size());
        submesh.indexCount = uint32_t(indices.size());
        submesh.baseVertex = uint32_t(out.positions.size());
        submesh.vertexCount = uint32_t(vertices.size());
        submesh.material = uint32_t(out.materials.size());

        glm::vec3 boundsMin(std::numeric_limits<float>::max());
        glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
        for (const Vertex& v : vertices)
        {
            boundsMin = glm::min(boundsMin, v.position);
            boundsMax = glm::max(boundsMax, v.position);

            // The file's UVs start at the top left; tangents were generated before this flip.
            out.positions.push_back({ { v.position.x, v.position.y, v.position.z } });
            out.attributes.push_back({
                .normal = { v.normal.x, v.normal.y, v.normal.z },
                .tangent = { v.tangent.x, v.tangent.y, v.tangent.z, v.tangent.w },
                .uv = { v.uv.x, 1.0f - v.uv.y },
                .color = { v.color.r, v.color.g, v.color.b, v.color.a },
                });
            if (skinned)
            {
                out.skin.push_back(QuantizeSkin(v));
            }
        }
        Store(boundsMin, submesh.boundsMin);
        Store(boundsMax, submesh.boundsMax);

        out.boundsMin = glm::min(out.boundsMin, boundsMin);
        out.boundsMax = glm::max(out.boundsMax, boundsMax);
        out.indices.insert(out.indices.end(), indices.begin(), indices.end());
        out.submeshes.push_back(submesh);
        out.materials.push_back(material);
    }

    out.paletteJoints = std::move(palette.Joints());
    out.inverseBinds = std::move(palette.InverseBinds());
    return out;
}