#include <PCH/pch.h>
#include "AnimationSystem.h"

#include "ECS/ECS.h"
#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/AnimationResource.h"

#include "ECS/Entities/Entity.h"
#include "ECS/Entities/Components.h"

#include "Graphics/Vulkan/Model/Animation/AnimationLibrary.h"
#include "Graphics/Vulkan/Model/Animation/Animator.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/RenderGraph.h"

namespace Dog
{
    void AnimationSystem::Update(float dt)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto ar = ecs->GetResource<AnimationResource>();
        auto& al = rr->animationLibrary;
        //al->UpdateAnimations(dt);

        // loop over model components
        entt::registry& registry = ecs->GetRegistry();
        auto view = registry.view<TransformComponent, ModelComponent, AnimationComponent>();

        auto& bonesMatrices = ar->bonesMatrices;
        bonesMatrices.clear();

        uint32_t boneOffset = 0;
        for (auto entityHandle : view)
        {
            TransformComponent& tc = view.get<TransformComponent>(entityHandle);
            ModelComponent& mc = view.get<ModelComponent>(entityHandle);
            AnimationComponent& ac = view.get<AnimationComponent>(entityHandle);

            if (ac.AnimationIndex == 10001) continue;

            Animation* anim = al->GetAnimation(ac.AnimationIndex);
            Animator* animator = al->GetAnimator(ac.AnimationIndex);

            if (!anim || !animator)
                continue;

            if (ac.IsPlaying) {
                ac.AnimationTime += anim->GetTicksPerSecond() * dt;
                ac.AnimationTime = fmod(ac.AnimationTime, anim->GetDuration());
                glm::mat4 tr = tc.GetTransform();
                animator->UpdateAnimationInstant(ac.AnimationTime, ac.inPlace, tr);
            }

            const auto& finalMatrices = animator->GetFinalBoneVQS();
            bonesMatrices.insert(bonesMatrices.end(), finalMatrices.begin(), finalMatrices.end());
            ac.BoneOffset = boneOffset;
            boneOffset += static_cast<uint32_t>(finalMatrices.size());
        }

        // auto& rg = rr->renderGraph;
        // rg->AddPass(
        //     "UpdateAnimationUniform",
        //     [](RGPassBuilder& builder) {},
        //     [&](VkCommandBuffer cmd) 
        //     {
        //         auto renderingData = ecs->GetResource<RenderingResource>();
        //         auto animationData = ecs->GetResource<AnimationResource>();
        //         renderingData->instanceUniform->SetUniformData(animationData->bonesMatrices, 2, renderingData->currentFrameIndex);
        //     }
        // );
    }
}
