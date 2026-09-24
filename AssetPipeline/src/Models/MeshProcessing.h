#pragma once

#include "ImportedScene.h"

// GPU-ready geometry in the file's layout. Every submesh draws with one material.
struct ProcessedGeometry
{
    std::vector<ModelFile::Submesh>          submeshes;      // Submesh::material indexes `materials`
    std::vector<uint32_t>                    materials;      // imported material index of each material slot
    std::vector<ModelFile::Position>         positions;
    std::vector<ModelFile::VertexAttributes> attributes;
    std::vector<uint32_t>                    indices;        // relative to each submesh's baseVertex
    glm::vec3                                boundsMin{ 0.0f };
    glm::vec3                                boundsMax{ 0.0f };
};

// Bakes every instance into engine space and merges the geometry that shares a material into one
// submesh. Tangents are generated (MikkTSpace) for materials with a normal map; then each submesh
// is welded and reordered for the GPU's vertex cache, overdraw and vertex fetch.
ProcessedGeometry ProcessMeshes(const ImportedScene& scene);