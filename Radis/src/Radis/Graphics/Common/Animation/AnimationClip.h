/*****************************************************************//**
 * \file   AnimationClip.h
 * \brief  An animation clip cooked by the asset pipeline.
 *********************************************************************/

#pragma once

#include "Graphics/Common/AnimationFormat.h"
#include "Skeleton.h"

namespace Radis
{
    // A cooked clip (see AnimationFormat.h), sampled uniformly. Its values are kept frame-major, so
    // sampling reads two frames that each sit together in memory.
    class AnimationClip
    {
    public:
        // Reads, decodes and checks a clip. Errors are logged.
        static std::unique_ptr<AnimationClip> Load(const std::string& path);

        const std::string& GetPath() const { return mPath; }
        float GetDuration() const { return mDuration; }
        uint32_t GetSkeletonHash() const { return mSkeletonHash; }
        size_t GetJointLimit() const { return mJointLimit; }   // one past the highest joint a track moves
        bool HasRootMotion() const { return !mRootMotion.empty(); }

        // Overwrites the joints the clip moves with their pose at `time` (clamped to the clip). The
        // other joints keep what `pose` holds, normally the rest pose.
        void Sample(float time, std::span<JointPose> pose) const;

        // How far the root joint has moved at `time`, in model space; zero without root motion.
        glm::vec3 SampleRootMotion(float time) const;

    private:
        struct Track
        {
            uint16_t               joint;
            AnimationFile::Channel channel;
        };

        // The two frames around `time` and how far between them it is.
        void FramesAt(float time, uint32_t& first, uint32_t& second, float& blend) const;
        static void Apply(const Track& track, const glm::vec4& value, std::span<JointPose> pose);

        std::string            mPath;
        uint32_t               mSkeletonHash = 0;
        uint32_t               mFrameCount = 1;
        float                  mDuration = 0.0f;
        size_t                 mJointLimit = 0;

        std::vector<Track>     mConstantTracks;
        std::vector<glm::vec4> mConstants;        // one per constant track
        std::vector<Track>     mAnimatedTracks;
        std::vector<glm::vec4> mFrames;           // a row of animated track values per frame
        std::vector<glm::vec4> mRootMotion;       // one per frame, or empty
    };
}
