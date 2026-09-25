#pragma once

#include "ModelFormat.h"

// Layout of a cooked animation clip (.da), which animates the skeleton of a cooked model (see
// ModelFormat.h). Shared by the asset pipeline and the engine, like ModelFormat.h, and built the
// same way: a Header, then Header::sectionCount Sections, then the data those sections point at.
//
// A clip is sampled uniformly: frame f is at time duration * f / (frameCount - 1). A joint channel
// without a track keeps the rest pose. A track holds either one value or one value per frame;
// rotations are xyzw, and neighbouring frames are in the same hemisphere, so a normalized lerp
// between them is safe.
namespace AnimationFile
{
    inline constexpr uint32_t kMagic = 0x4D4E4152;   // "RANM" as bytes in the file
    inline constexpr uint16_t kVersion = 1;

    enum class SectionType : uint32_t
    {
        Tracks,       // Track[]
        Constants,    // float[4] per constant track
        Samples,      // float[4] per frame of each animated track: track-major, frameCount values per track
        RootMotion,   // float[4] per frame: the motion taken out of the root joint (xyz); only when Header::rootJoint >= 0
    };

    struct Header
    {
        uint32_t magic;
        uint16_t version;
        uint16_t sectionCount;
        uint32_t skeletonHash;   // SkeletonHash of the skeleton the clip was cooked for
        uint32_t frameCount;     // at least 1
        float    duration;       // seconds
        float    sampleRate;     // frames per second the source was sampled at
        int16_t  rootJoint;      // the joint whose horizontal motion is in RootMotion, -1 for none
        uint16_t reserved[3];
    };

    // Same layout and meaning as ModelFile::Section.
    struct Section
    {
        SectionType      type;
        ModelFile::Codec codec;
        uint32_t         elementSize;
        uint32_t         elementCount;
        uint64_t         offset;
        uint64_t         size;
    };

    enum class Channel : uint8_t { Translation, Rotation, Scale };

    struct Track
    {
        uint16_t joint;
        Channel  channel;
        uint8_t  animated;   // 1: `index` is an animated track in Samples; 0: an entry in Constants
        uint32_t index;
    };

    // Identifies a skeleton by its joints' names and parents, which is what a clip's tracks rely on.
    inline uint32_t SkeletonHash(const ModelFile::Joint* joints, size_t count)
    {
        uint32_t   hash = 2166136261u;
        const auto mix = [&](uint32_t value)
            {
                for (int i = 0; i < 4; ++i)
                {
                    hash = (hash ^ ((value >> (i * 8)) & 0xFF)) * 16777619u;
                }
            };

        for (size_t i = 0; i < count; ++i)
        {
            mix(joints[i].nameHash);
            mix(uint32_t(int32_t(joints[i].parent)));
        }
        return hash;
    }

    static_assert(sizeof(Header) == 32 && sizeof(Section) == 32 && sizeof(Track) == 8);
}
