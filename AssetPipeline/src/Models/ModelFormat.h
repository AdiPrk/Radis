#pragma once

#include <cstddef>
#include <cstdint>

// Layout of a cooked model file (.dm). Shared by the asset pipeline, which writes it, and the
// engine, which loads it, so it has no other dependencies.
//
// The file is a Header, then Header::sectionCount Sections, then the data those sections point at,
// each block aligned to kSectionAlignment. All values are little-endian; offsets are from the start
// of the file. A different version means the model must be re-cooked; there's no conversion.
//
// Conventions: Y-up, right-handed, meters. UV (0, 0) is the top left of a texture. Front faces are
// counter-clockwise. Normal maps are +Y (OpenGL style) and bitangent = cross(normal, tangent.xyz) * tangent.w.
//
// Skinned models also have the four skin sections. A vertex's skinned position is the weighted sum
// of jointModel[SkinJoints[p]] * InverseBinds[p] * position over its palette entries p, where
// jointModel is a joint's transform from its space to model space. In the rest pose that gives back
// the stored position.
namespace ModelFile
{
    inline constexpr uint32_t kMagic = 0x4C444D52;   // "RMDL" as bytes in the file
    inline constexpr uint16_t kVersion = 3;
    inline constexpr uint32_t kSectionAlignment = 16;

    enum class SectionType : uint32_t
    {
        Submeshes,    // Submesh[]
        Materials,    // Material[]
        Positions,    // Position[], one per vertex
        Attributes,   // VertexAttributes[], one per vertex
        Indices,      // uint16_t or uint32_t (see elementSize), relative to each submesh's baseVertex
        Strings,      // char[]: null-terminated UTF-8 strings, back to back
        Skeleton,     // Joint[]; skinned models only, like the three below
        SkinJoints,   // uint16_t[]: the joint each palette entry follows
        InverseBinds, // Matrix3x4[], one per palette entry
        SkinWeights,  // SkinWeights[], one per vertex
    };

    enum class Codec : uint32_t
    {
        None,
        MeshoptVertex,   // meshopt_decodeVertexBuffer(destination, elementCount, elementSize, data, size)
        MeshoptIndex,    // meshopt_decodeIndexBuffer(destination, elementCount, elementSize, data, size)
    };

    struct Header
    {
        uint32_t magic;
        uint16_t version;
        uint16_t sectionCount;
        float    boundsMin[3];
        float    boundsMax[3];
    };

    struct Section
    {
        SectionType type;
        Codec       codec;
        uint32_t    elementSize;    // bytes per element once decoded
        uint32_t    elementCount;
        uint64_t    offset;
        uint64_t    size;           // bytes stored in the file
    };

    // A draw: indices [firstIndex, firstIndex + indexCount) with vertices starting at baseVertex.
    struct Submesh
    {
        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t baseVertex;
        uint32_t vertexCount;
        uint32_t material;          // index into the Materials section
        float    boundsMin[3];
        float    boundsMax[3];
    };

    enum class AlphaMode : uint8_t { Opaque, Mask, Blend };
    enum class TextureWrap : uint8_t { Repeat, ClampToEdge, MirroredRepeat };

    // Texture slots. ORM holds occlusion in R, roughness in G and metalness in B (glTF's layout).
    enum class MaterialTexture : uint8_t { BaseColor, Normal, ORM, Emissive, Transmission, Count };
    inline constexpr size_t kMaterialTextureCount = size_t(MaterialTexture::Count);

    inline constexpr uint32_t kNoTexture = 0xFFFFFFFF;

    // Factors multiply their texture (glTF's metallic-roughness model); with no texture, the factor
    // alone applies.
    struct Material
    {
        uint32_t    textures[kMaterialTextureCount];   // offsets into Strings of texture file names, kNoTexture for none;
        // the files are in the "Textures" folder inside the model's folder
        float       baseColor[4];                      // linear RGBA
        float       emissive[3];                       // linear RGB
        float       emissiveStrength;
        float       metallic;
        float       roughness;
        float       occlusionStrength;                 // occlusion = lerp(1, ORM.r, occlusionStrength)
        float       normalScale;                       // scales the normal map's X and Y
        float       transmission;
        float       ior;
        float       alphaCutoff;                       // for AlphaMode::Mask
        AlphaMode   alphaMode;
        uint8_t     doubleSided;
        TextureWrap wrapU[kMaterialTextureCount];
        TextureWrap wrapV[kMaterialTextureCount];
    };

    struct Position
    {
        float xyz[3];
    };

    struct VertexAttributes
    {
        float normal[3];
        float tangent[4];   // xyz, and the bitangent sign in w
        float uv[2];
        float color[4];     // linear; multiplies the base color
    };

    // A row-major affine transform: the rows of the top three rows of a 4x4 matrix.
    struct Matrix3x4
    {
        float rows[3][4];
    };

    // Joints are in depth-first order, so a joint's parent always comes before it.
    struct Joint
    {
        int16_t  parent;            // -1 for a root
        uint16_t reserved;
        uint32_t name;              // offset into Strings
        uint32_t nameHash;          // NameHash(name)
        float    translation[3];    // rest pose, relative to the parent
        float    rotation[4];       // xyzw
        float    scale[3];
    };

    // Four influences, most significant first. Joints index the palette (SkinJoints); weights are
    // unorm16 and sum to 65535, except on a vertex that isn't skinned, where they're all 0.
    struct SkinWeights
    {
        uint16_t joints[4];
        uint16_t weights[4];
    };

    // FNV-1a of a joint's name, for finding joints without comparing strings.
    constexpr uint32_t NameHash(const char* name)
    {
        uint32_t hash = 2166136261u;
        for (; *name; ++name)
        {
            hash = (hash ^ uint8_t(*name)) * 16777619u;
        }
        return hash;
    }

    static_assert(sizeof(Header) == 32 && sizeof(Section) == 32 && sizeof(Submesh) == 44);
    static_assert(sizeof(Material) == 92 && sizeof(Position) == 12 && sizeof(VertexAttributes) == 52);
    static_assert(sizeof(Matrix3x4) == 48 && sizeof(Joint) == 52 && sizeof(SkinWeights) == 16);
}