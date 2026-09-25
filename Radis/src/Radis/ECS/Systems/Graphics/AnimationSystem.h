/*****************************************************************//**
 * \file   AnimationSystem.h
 * \brief  Handles animating entities
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../ISystem.h"

namespace Radis
{
    class AnimationClip;
    struct Skeleton;

    class AnimationSystem : public ISystem
    {
    public:
        AnimationSystem() : ISystem("AnimationSystem") {};
        ~AnimationSystem() {}

        void Update(float dt);

    private:
        // One animated entity's work for the frame.
        struct Job
        {
            const Skeleton*      skeleton;
            const AnimationClip* clip;
            float                time;
            glm::vec3            offset;        // root motion that moves the whole pose
            size_t               firstJoint;    // into mModelPoses
            uint32_t             boneOffset;    // into AnimationResource::skinMatrices
            bool                 drawSkeleton;
            glm::mat4            transform;     // the entity's, for drawing the skeleton
        };

        void DrawSkeleton(const Job& job) const;

        std::vector<Job>                mJobs;
        std::vector<glm::mat4>          mModelPoses;   // every job's joints, relative to its model
        std::unordered_set<std::string> mWarnings;     // already reported, so they're reported once
    };
}
