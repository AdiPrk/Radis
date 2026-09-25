/*****************************************************************//**
 * \file   AnimationSystem.cpp
 * \brief  Handles animating entities
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "AnimationSystem.h"

#include "ECS/ECS.h"
#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/AnimationResource.h"
#include "ECS/Resources/DebugDrawResource.h"

#include "ECS/Components/Components.h"

#include "Graphics/Common/Animation/AnimationClip.h"
#include "Graphics/Common/Animation/AnimationLibrary.h"
#include "Graphics/Common/Model.h"
#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

#include <execution>

namespace Radis
{
    // The clip the component names, loaded on first use. The ID is checked against the path, since
    // a scene may have been saved with an ID from another run.
    static const AnimationClip* ResolveClip(AnimationLibrary& library, AnimationComponent& ac)
    {
        if (ac.ClipPath.empty())
            return nullptr;

        const AnimationClip* clip = library.GetClip(ac.ClipID);
        if (!clip || clip->GetPath() != ac.ClipPath)
        {
            ac.ClipID = library.AddClip(ac.ClipPath);
            clip = library.GetClip(ac.ClipID);
        }
        return clip;
    }

    void AnimationSystem::Update(float dt)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto ar = ecs->GetResource<AnimationResource>();
        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

        auto& skinMatrices = ar->skinMatrices;
        skinMatrices.clear();
        mJobs.clear();
        size_t jointCount = 0;

        // Advance time and hand out palette space serially; the pose math below runs in parallel.
        entt::registry& registry = ecs->GetRegistry();
        registry.view<TransformComponent, ModelComponent, AnimationComponent>().each(
            [&](TransformComponent& tc, ModelComponent& mc, AnimationComponent& ac)
        {
            ac.BoneOffset = AnimationLibrary::INVALID_ANIMATION_INDEX;

            const Model* model = ml->GetModel(mc);
            const Skeleton* skeleton = model ? model->GetSkeleton() : nullptr;
            const AnimationClip* clip = ResolveClip(*al, ac);
            if (!skeleton || !clip)
                return;

            if (clip->GetSkeletonHash() != skeleton->hash || clip->GetJointLimit() > skeleton->JointCount())
            {
                if (mWarnings.insert(clip->GetPath() + "|" + model->GetName()).second)
                {
                    RADIS_WARN("{} was cooked for a different skeleton than {} has", clip->GetPath(), model->GetName());
                }
                return;
            }
            if (skinMatrices.size() + skeleton->PaletteSize() > AnimationUniforms::MAX_BONES)
            {
                if (mWarnings.insert("MAX_BONES").second)
                {
                    RADIS_WARN("More than {} skinning matrices in a frame; the entities past that aren't animated", AnimationUniforms::MAX_BONES);
                }
                return;
            }

            if (ac.IsPlaying)
            {
                ac.Time += dt * ac.Speed;
            }
            const float duration = clip->GetDuration();
            if (duration <= 0.0f)
            {
                ac.Time = 0.0f;
            }
            else if (ac.Loop)
            {
                ac.Time = std::fmod(ac.Time, duration);
                if (ac.Time < 0.0f) ac.Time += duration;
            }
            else
            {
                ac.Time = std::clamp(ac.Time, 0.0f, duration);
            }

            ac.BoneOffset = static_cast<uint32_t>(skinMatrices.size());
            skinMatrices.resize(skinMatrices.size() + skeleton->PaletteSize());

            const glm::vec3 offset = ac.InPlace ? glm::vec3(0.0f) : clip->SampleRootMotion(ac.Time);
            const glm::mat4 transform = mc.NormalizeModel ? tc.GetTransform() * model->GetNormalizationMatrix() : tc.GetTransform();
            mJobs.push_back({ skeleton, clip, ac.Time, offset, jointCount, ac.BoneOffset, ac.DrawSkeleton, transform });
            jointCount += skeleton->JointCount();
        });

        mModelPoses.resize(jointCount);
        std::for_each(std::execution::par, mJobs.begin(), mJobs.end(), [&](const Job& job)
        {
            thread_local std::vector<JointPose> local;
            local.assign(job.skeleton->restPose.begin(), job.skeleton->restPose.end());
            job.clip->Sample(job.time, local);

            const std::span<glm::mat4> model(mModelPoses.data() + job.firstJoint, job.skeleton->JointCount());
            LocalToModel(*job.skeleton, local, model);
            BuildSkinPalette(*job.skeleton, model, job.offset, std::span(skinMatrices).subspan(job.boneOffset, job.skeleton->PaletteSize()));
        });

        // Debug drawing isn't thread safe, so skeletons are drawn afterwards.
        for (const Job& job : mJobs)
        {
            if (job.drawSkeleton)
            {
                DrawSkeleton(job);
            }
        }
    }

    void AnimationSystem::DrawSkeleton(const Job& job) const
    {
        const Skeleton& skeleton = *job.skeleton;
        const auto position = [&](size_t joint)
        {
            return glm::vec3(job.transform * glm::vec4(glm::vec3(mModelPoses[job.firstJoint + joint][3]) + job.offset, 1.0f));
        };

        for (size_t i = 0; i < skeleton.JointCount(); ++i)
        {
            const glm::vec3 end = position(i);
            if (skeleton.parents[i] >= 0)
            {
                DebugDrawResource::DrawLine(position(size_t(skeleton.parents[i])), end, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
            }
            DebugDrawResource::DrawCube(end, glm::vec3(0.01f), glm::vec4(0.0f, 1.0f, 1.0f, 0.4f));
        }
    }
}
