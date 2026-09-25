#include <pch.h>
#include "ClipWriter.h"
#include "SectionFileBuilder.h"
#include "../FileIO.h"

std::expected<void, std::string> WriteClip(const std::filesystem::path& path, const ProcessedClip& clip)
{
    const bool     rootMotion = clip.rootJoint >= 0;
    const uint16_t sectionCount = rootMotion ? 4 : 3;

    // Samples are track-major, so the vertex codec sees each track's frames one after another.
    SectionFileBuilder<AnimationFile::Header, AnimationFile::Section> builder(sectionCount);
    builder.AddRaw(AnimationFile::SectionType::Tracks, std::span(clip.tracks));
    builder.AddRaw(AnimationFile::SectionType::Constants, std::span(clip.constants));
    builder.AddVertices(AnimationFile::SectionType::Samples, std::span(clip.samples));
    if (rootMotion)
    {
        builder.AddVertices(AnimationFile::SectionType::RootMotion, std::span(clip.rootMotion));
    }

    AnimationFile::Header header{};
    header.magic = AnimationFile::kMagic;
    header.version = AnimationFile::kVersion;
    header.sectionCount = sectionCount;
    header.skeletonHash = clip.skeletonHash;
    header.frameCount = clip.frameCount;
    header.duration = clip.duration;
    header.sampleRate = clip.sampleRate;
    header.rootJoint = int16_t(clip.rootJoint);

    return WriteFileAtomic(path, builder.Finish(header));
}
