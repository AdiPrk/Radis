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
};
static_assert(sizeof(Vertex) == 64);

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

// Appends the instance's triangles as unshared corners (3 per triangle) in engine space.
static void AppendInstance(const ImportedMesh& mesh, const glm::mat4& transform, std::vector<Vertex>& corners)
{
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

    // A mirroring transform turns the triangles inside out; swapping two corners restores the winding.
    const bool mirrored = glm::determinant(linear) < 0.0f;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        corners.push_back(vertices[mesh.indices[i]]);
        corners.push_back(vertices[mesh.indices[i + (mirrored ? 2 : 1)]]);
        corners.push_back(vertices[mesh.indices[i + (mirrored ? 1 : 2)]]);
    }
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

    for (const auto& [material, instances] : byMaterial)
    {
        std::vector<Vertex> corners;
        for (const ImportedInstance* instance : instances)
        {
            AppendInstance(scene.meshes[instance->mesh], instance->transform, corners);
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
        }
        Store(boundsMin, submesh.boundsMin);
        Store(boundsMax, submesh.boundsMax);

        out.boundsMin = glm::min(out.boundsMin, boundsMin);
        out.boundsMax = glm::max(out.boundsMax, boundsMax);
        out.indices.insert(out.indices.end(), indices.begin(), indices.end());
        out.submeshes.push_back(submesh);
        out.materials.push_back(material);
    }
    return out;
}