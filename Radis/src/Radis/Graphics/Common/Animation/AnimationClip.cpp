/*****************************************************************//**
 * \file   AnimationClip.cpp
 * \brief  An animation clip cooked by the asset pipeline.
 *********************************************************************/

#include <PCH/pch.h>
#include "AnimationClip.h"
#include "Graphics/Common/CookedFile.h"

namespace Radis
{
    namespace
    {
        using namespace AnimationFile;

        constexpr const char* kSectionNames[] = { "Tracks", "Constants", "Samples", "RootMotion" };

        const char* SectionName(SectionType type)
        {
            return size_t(type) < std::size(kSectionNames) ? kSectionNames[size_t(type)] : "unknown";
        }
    }

    std::unique_ptr<AnimationClip> AnimationClip::Load(const std::string& path)
    {
        std::vector<std::byte> file;
        if (!CookedFile::ReadFile(path, file))
        {
            return nullptr;
        }

        Header header{};
        if (file.size() >= sizeof(Header))
        {
            std::memcpy(&header, file.data(), sizeof(Header));
        }
        if (header.magic != kMagic)
        {
            RADIS_ERROR("{}: not a cooked animation clip", path);
            return nullptr;
        }
        if (header.version != kVersion)
        {
            RADIS_ERROR("{}: cooked with clip format version {}, the engine reads version {}; re-cook it", path, header.version, kVersion);
            return nullptr;
        }

        const size_t tableEnd = sizeof(Header) + sizeof(Section) * header.sectionCount;
        if (file.size() < tableEnd || header.frameCount == 0)
        {
            RADIS_ERROR("{}: the header or section table is broken", path);
            return nullptr;
        }

        std::vector<AnimationFile::Track> tracks;
        std::vector<glm::vec4>            samples;
        auto                              clip = std::make_unique<AnimationClip>();
        bool                              found[std::size(kSectionNames)] = {};
        for (uint16_t i = 0; i < header.sectionCount; ++i)
        {
            Section section{};
            std::memcpy(&section, file.data() + sizeof(Header) + sizeof(Section) * i, sizeof(Section));
            if (section.offset > file.size() || section.size > file.size() - section.offset)
            {
                RADIS_ERROR("{}: the {} section lies outside the file", path, SectionName(section.type));
                return nullptr;
            }

            ModelFile::Section common{};   // the same layout, which the decoder reads
            std::memcpy(&common, &section, sizeof(common));

            const std::byte* data = file.data() + section.offset;
            const char*      name = SectionName(section.type);
            bool             decoded = false;
            switch (section.type)
            {
            case SectionType::Tracks:     decoded = CookedFile::DecodeArray(common, data, tracks, path, name);           break;
            case SectionType::Constants:  decoded = CookedFile::DecodeArray(common, data, clip->mConstants, path, name); break;
            case SectionType::Samples:    decoded = CookedFile::DecodeArray(common, data, samples, path, name);          break;
            case SectionType::RootMotion: decoded = CookedFile::DecodeArray(common, data, clip->mRootMotion, path, name); break;
            default:                      continue;   // sections this engine doesn't know about are skipped
            }

            if (!decoded)
            {
                return nullptr;
            }
            found[size_t(section.type)] = true;
        }

        const bool rootMotion = header.rootJoint >= 0;
        if (!found[size_t(SectionType::Tracks)] || !found[size_t(SectionType::Constants)] || !found[size_t(SectionType::Samples)]
            || found[size_t(SectionType::RootMotion)] != rootMotion || (rootMotion && clip->mRootMotion.size() != header.frameCount)
            || samples.size() % header.frameCount != 0)
        {
            RADIS_ERROR("{}: the clip's sections don't fit together", path);
            return nullptr;
        }

        // Tracks split by kind; the animated ones' samples go from track-major to frame-major.
        const size_t animatedCount = samples.size() / header.frameCount;
        for (const AnimationFile::Track& track : tracks)
        {
            if (size_t(track.channel) > size_t(Channel::Scale) || track.index >= (track.animated ? animatedCount : clip->mConstants.size()))
            {
                RADIS_ERROR("{}: a track refers outside the clip", path);
                return nullptr;
            }

            const AnimationClip::Track kept{ track.joint, track.channel };
            if (track.animated)
            {
                clip->mAnimatedTracks.push_back(kept);
            }
            else
            {
                clip->mConstantTracks.push_back(kept);
            }
            clip->mJointLimit = std::max(clip->mJointLimit, size_t(track.joint) + 1);
        }

        // Each animated track becomes a column, in the order of mAnimatedTracks.
        clip->mFrames.resize(samples.size());
        size_t column = 0;
        for (const AnimationFile::Track& track : tracks)
        {
            if (!track.animated)
                continue;

            for (uint32_t f = 0; f < header.frameCount; ++f)
            {
                clip->mFrames[f * animatedCount + column] = samples[size_t(track.index) * header.frameCount + f];
            }
            ++column;
        }

        // And constants line up with mConstantTracks.
        std::vector<glm::vec4> constants;
        for (const AnimationFile::Track& track : tracks)
        {
            if (!track.animated)
            {
                constants.push_back(clip->mConstants[track.index]);
            }
        }
        clip->mConstants = std::move(constants);

        clip->mPath = path;
        clip->mSkeletonHash = header.skeletonHash;
        clip->mFrameCount = header.frameCount;
        clip->mDuration = header.duration;
        return clip;
    }

    void AnimationClip::FramesAt(float time, uint32_t& first, uint32_t& second, float& blend) const
    {
        const float frame = mFrameCount > 1 && mDuration > 0.0f ? std::clamp(time / mDuration, 0.0f, 1.0f) * float(mFrameCount - 1) : 0.0f;
        first = std::min(uint32_t(frame), mFrameCount - 1);
        second = std::min(first + 1, mFrameCount - 1);
        blend = frame - float(first);
    }

    void AnimationClip::Apply(const Track& track, const glm::vec4& value, std::span<JointPose> pose)
    {
        JointPose& joint = pose[track.joint];
        switch (track.channel)
        {
        case AnimationFile::Channel::Translation: joint.translation = glm::vec3(value); break;
        case AnimationFile::Channel::Rotation:    joint.rotation = glm::normalize(glm::quat(value.w, value.x, value.y, value.z)); break;
        case AnimationFile::Channel::Scale:       joint.scale = glm::vec3(value); break;
        }
    }

    void AnimationClip::Sample(float time, std::span<JointPose> pose) const
    {
        for (size_t i = 0; i < mConstantTracks.size(); ++i)
        {
            Apply(mConstantTracks[i], mConstants[i], pose);
        }

        uint32_t first, second;
        float    blend;
        FramesAt(time, first, second, blend);

        // Neighbouring rotations are in the same hemisphere, so a lerp that's normalized is enough.
        const size_t     count = mAnimatedTracks.size();
        const glm::vec4* a = mFrames.data() + first * count;
        const glm::vec4* b = mFrames.data() + second * count;
        for (size_t i = 0; i < count; ++i)
        {
            Apply(mAnimatedTracks[i], glm::mix(a[i], b[i], blend), pose);
        }
    }

    glm::vec3 AnimationClip::SampleRootMotion(float time) const
    {
        if (mRootMotion.empty())
            return glm::vec3(0.0f);

        uint32_t first, second;
        float    blend;
        FramesAt(time, first, second, blend);
        return glm::vec3(glm::mix(mRootMotion[first], mRootMotion[second], blend));
    }
}
