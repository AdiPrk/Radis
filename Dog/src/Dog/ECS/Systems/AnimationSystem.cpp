#include <PCH/pch.h>
#include "AnimationSystem.h"

#include "ECS/ECS.h"
#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/AnimationResource.h"
#include "ECS/Resources/DebugDrawResource.h"

#include "ECS/Entities/Entity.h"
#include "ECS/Entities/Components.h"

#include "Graphics/Common/Animation/AnimationLibrary.h"
#include "Graphics/Common/Animation/Animator.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/RenderGraph.h"

#include "Graphics/Common/Path/PathFollower.h"
#include "Graphics/Common/Path/ArcLengthTable.h"
#include "Graphics/Common/Path/SpeedControl.h"
#include "Graphics/Common/Path/CubicSpline.h"

#include "Engine.h"

namespace Dog
{
    void AnimationSystem::Update(float dt)
    {
        std::vector<glm::vec3> controlPoints = {
            { 0, 0,  2}, // P0
            { 2, 0,  2}, // P1
            { 2, 0,  0}, // P2
            { 2, 0, -2}, // P3
            { 0, 0, -2}, // P4
            {-2, 0, -2}, // P5
            {-2, 0,  0}, // P6
            {-2, 0,  2}, // P7
            { 0, 0,  2}, // P8 = P0
            { 2, 0,  2}, // P9 = P1
            { 2, 0,  0}  // P10 = P2
        };
        // This will create (11 - 3) = 8 spline segments.

        // 2. Create and build the PathFollower
        PathFollower pathFollower;

        // Set precision: (epsilon, delta)
        // Smaller values = more accurate, slower build time.
        pathFollower.BuildPath(controlPoints, 0.001, 0.01);

        // 3. Set the speed profile
        // Ease in for first 15% of time, ease out at 85% of time
        pathFollower.SetSpeedProfile(0.15, 0.85);

        const CubicSpline& spline = pathFollower.GetSpline();
        for (int i = 0; i < spline.GetNumSegments(); ++i) {
            for (double t = 0; t < 1.0; t += 0.05) {
                glm::vec3 p1 = spline.GetPointOnSegment(i, t);
                glm::vec3 p2 = spline.GetPointOnSegment(i, t + 0.05);
                DebugDrawResource::DrawLine(p1, p2, glm::vec4(1.0f), 0.05f);
            }
        }

        static float t_norm = 0.0f;
        t_norm += 0.12f * dt;
        if (t_norm > 1.0f) t_norm -= 1.0f;

        /*
        const int trailLength = 200;
        for (int i = 0; i < trailLength; ++i)
        {
            float incI = t_norm + i * 0.002f;
            if (incI > 1.0f) incI -= 1.0f;

            // Smooth oscillation for size (sin-based)
            float sizePulse = 0.25f + 0.2f * sinf((float)i * 0.1f + t_norm * 6.2831f);
            sizePulse += i * 0.002f; // gradually grow along the trail

            // Fun color variation using sine waves for R, G, B
            float r = 0.5f + 0.5f * sinf((float)i * 0.1f + t_norm * 6.2831f);
            float g = 0.5f + 0.5f * sinf((float)i * 0.15f + t_norm * 6.2831f + 2.0f);
            float b = 0.5f + 0.5f * sinf((float)i * 0.2f + t_norm * 6.2831f + 4.0f);

            glm::vec4 color(r, g, b, 1.0f);

            const glm::mat4 characterTransform = pathFollower.GetTransformAtTime(incI, { 0, 1, 0 });
            DebugDrawResource::DrawRect(
                glm::vec3(characterTransform[3]),
                glm::vec2(sizePulse, sizePulse),
                color
            );
        }
        */

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
                const double worldSpeed = pathFollower.GetCurrentWorldSpeed(t_norm, anim->GetDuration());

                // 2. Calculate the playback multiplier
                double speedMultiplier = 0.0;
                float moveSpeed = 1.8f;
                if (moveSpeed > 1e-5) // Avoid divide by zero
                {
                    speedMultiplier = worldSpeed / moveSpeed;
                }

                ac.AnimationTime += anim->GetTicksPerSecond() * dt * (float)speedMultiplier;
                ac.AnimationTime = fmod(ac.AnimationTime, anim->GetDuration());

                glm::mat4 characterTransform = pathFollower.GetTransformAtTime(t_norm, { 0, 1, 0 });
                VQS vqs(characterTransform);
                tc.Translation = vqs.translation;
                tc.Rotation = glm::eulerAngles(vqs.rotation);
                glm::mat4 tr = tc.GetTransform();

                animator->UpdateAnimationInstant(ac.AnimationTime, ac.inPlace, tr);
            }

            const auto& finalMatrices = animator->GetFinalBoneVQS();
            bonesMatrices.insert(bonesMatrices.end(), finalMatrices.begin(), finalMatrices.end());
            ac.BoneOffset = boneOffset;
            boneOffset += static_cast<uint32_t>(finalMatrices.size());
        }

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto& rg = rr->renderGraph;
            rg->AddPass(
                "UpdateAnimationUniform",
                [](RGPassBuilder& builder) {},
                [&](VkCommandBuffer cmd)
                {
                    auto renderingData = ecs->GetResource<RenderingResource>();
                    auto animationData = ecs->GetResource<AnimationResource>();
                    renderingData->cameraUniform->SetUniformData(animationData->bonesMatrices, 2, renderingData->currentFrameIndex);
                }
            );
        }
        else
        {
            rr->shader->Use();
            GLShader::SetupAnimationSSBO();
            GLuint animationVBO = GLShader::GetAnimationSSBO();
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, animationVBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER, bonesMatrices.size() * sizeof(VQS), bonesMatrices.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
    }
}
