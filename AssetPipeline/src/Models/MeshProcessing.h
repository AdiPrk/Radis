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

    // Skinned models only (the scene has a skeleton).
    std::vector<ModelFile::SkinWeights>      skin;           // one per vertex
    std::vector<uint16_t>                    paletteJoints;  // the joint each palette entry follows
    std::vector<glm::mat4>                   inverseBinds;   // one per palette entry, from model space to the joint's
    float                                    droppedWeight = 0.0f;   // the largest share of a vertex's weight dropped to keep 4 influences
};

// Bakes every instance into engine space and merges the geometry that shares a material into one
// submesh. Tangents are generated (MikkTSpace) for materials with a normal map; then each submesh
// is welded and reordered for the GPU's vertex cache, overdraw and vertex fetch.
//
// In a skinned model, bones are rebased onto the baked vertices, and a mesh without bones follows
// its nearest joint rigidly, so the rest pose matches the baked geometry.
ProcessedGeometry ProcessMeshes(const ImportedScene& scene);