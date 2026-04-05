/*****************************************************************//**
 * \file   CameraSystem.cpp
 * \brief  The camera controller
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "CameraSystem.h"
#include "InputSystem.h"
#include "ECS/ECS.h"
#include "ECS/Components/Components.h"

namespace Radis
{

    // ====================================================================
    // Internal helpers
    // ====================================================================

    namespace
    {
        /// Shortest signed angular difference (to - from) in [-180, 180).
        [[nodiscard]] inline float AngleDiffDeg(float from, float to) noexcept
        {
            return std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
        }

        /// Shortest-path lerp between two degree angles.
        [[nodiscard]] inline float LerpAngleDeg(float a, float b, float t) noexcept
        {
            return a + AngleDiffDeg(a, b) * t;
        }

        /**
         * Frame-rate-independent exponential smoothing factor.
         *   alpha = 1 - e^(-k * dt)
         *   k > 0 : larger -> faster convergence / less smoothing
         *   k = 0 : no smoothing (alpha = 0, value never changes)
         */
        [[nodiscard]] inline float SmoothAlpha(float k, float dt) noexcept
        {
            return 1.0f - std::exp(-k * dt);
        }

    } // anonymous namespace

    // ====================================================================
    // CameraSystem::Update
    // ====================================================================

    void CameraSystem::Update(float dt)
    {
        if (dt <= 0.0f) [[unlikely]] return;

        static constexpr float     kMaxPitchDeg = 89.9f;               // avoid singularity at ±90°
        static constexpr float     kDeadZoneSq = 1e-6f;               // move-dir length² threshold
        static constexpr glm::vec3 kWorldUp = { 0.0f, 1.0f, 0.0f };

        entt::registry& registry = ecs->GetRegistry();
        auto view = registry.view<TransformComponent, CameraComponent>();

        for (auto entityHandle : view)
        {
            Entity entity(&registry, entityHandle);
            auto& transform = entity.GetComponent<TransformComponent>();
            auto& camera = entity.GetComponent<CameraComponent>();

            // ----------------------------------------------------------------
            // One-time initialization
            // ----------------------------------------------------------------
            if (!camera.IsInitialized) [[unlikely]]
            {
                camera.TargetYaw = transform.Rotation.y;
                camera.TargetPitch = transform.Rotation.x;
                camera.SmoothedYaw = camera.TargetYaw;
                camera.SmoothedPitch = camera.TargetPitch;
                camera.SmoothedVelocity = glm::vec3(0.0f);
                camera.IsInitialized = true;
            }

            // ----------------------------------------------------------------
            // Input state
            // ----------------------------------------------------------------
            const bool mouseHeld = InputSystem::isMouseDown(Mouse::RIGHT)
                && !InputSystem::IsMouseInputLocked();

            // ----------------------------------------------------------------
            // Rotation target accumulation
            //
            // Raw mouse delta is applied directly to the target angles.
            // Smoothing is applied on the *output* (SmoothedYaw/Pitch), never
            // on the input — smoothing both sides adds latency with no benefit.
            // ----------------------------------------------------------------
            if (mouseHeld) [[likely]]
            {
                const float invertSign = camera.InvertY ? -1.0f : 1.0f;

                camera.TargetYaw += InputSystem::GetMouseDeltaX() * camera.MouseSensitivity;
                camera.TargetPitch = glm::clamp(
                    camera.TargetPitch + InputSystem::GetMouseDeltaY() * camera.MouseSensitivity * invertSign,
                    -kMaxPitchDeg, kMaxPitchDeg);

                // Wrap yaw into (-360, 360) to prevent float precision drift over time
                camera.TargetYaw = std::fmod(camera.TargetYaw, 360.0f);
            }
            else
            {
                // Keep targets in sync with any externally driven rotation
                camera.TargetYaw = transform.Rotation.y;
                camera.TargetPitch = transform.Rotation.x;
            }

            // ----------------------------------------------------------------
            // Smooth rotation toward target  (frame-rate independent)
            // ----------------------------------------------------------------
            {
                const float rotAlpha = SmoothAlpha(camera.RotationSmoothness, dt);

                camera.SmoothedYaw = LerpAngleDeg(camera.SmoothedYaw, camera.TargetYaw, rotAlpha);
                camera.SmoothedPitch = glm::mix(camera.SmoothedPitch, camera.TargetPitch, rotAlpha);

                transform.Rotation = { camera.SmoothedPitch, camera.SmoothedYaw, 0.0f };
            }

            // ----------------------------------------------------------------
            // Basis vectors from smoothed Euler angles
            // ----------------------------------------------------------------
            glm::vec3 rightDir; // cached for movement below

            {
                const float yawRad = glm::radians(camera.SmoothedYaw);
                const float pitchRad = glm::radians(camera.SmoothedPitch);

                const float sinY = std::sin(yawRad), cosY = std::cos(yawRad);
                const float sinP = std::sin(pitchRad), cosP = std::cos(pitchRad);

                // Standard right-hand, Y-up camera forward
                camera.Forward = glm::normalize(glm::vec3(sinY * cosP, sinP, -cosY * cosP));

                rightDir = glm::cross(camera.Forward, kWorldUp);
                if (glm::length2(rightDir) < kDeadZoneSq) [[unlikely]]
                    rightDir = glm::vec3(1.0f, 0.0f, 0.0f);
                else
                    rightDir = glm::normalize(rightDir);

                camera.Up = glm::normalize(glm::cross(rightDir, camera.Forward));
            }

            // ----------------------------------------------------------------
            // Movement  —  velocity spring (frame-rate independent)
            //
            // targetVelocity = inputDir * speed  (zero when no input)
            // SmoothedVelocity lerps toward targetVelocity each frame
            // position += SmoothedVelocity * dt
            //
            // This is the standard approach: smooth the velocity, not the
            // position. Smoothing position toward (pos + vel*dt) silently
            // scales effective speed by alpha and is frame-rate dependent.
            // ----------------------------------------------------------------
            {
                glm::vec3 targetVelocity(0.0f);

                if (mouseHeld)
                {
                    glm::vec3 moveDir(0.0f);

                    if (InputSystem::isKeyDown(Key::W)) moveDir += camera.Forward;
                    if (InputSystem::isKeyDown(Key::S)) moveDir -= camera.Forward;
                    if (InputSystem::isKeyDown(Key::D)) moveDir += rightDir;
                    if (InputSystem::isKeyDown(Key::A)) moveDir -= rightDir;
                    if (InputSystem::isKeyDown(Key::E)) moveDir += kWorldUp;
                    if (InputSystem::isKeyDown(Key::Q)) moveDir -= kWorldUp;

                    if (glm::length2(moveDir) > kDeadZoneSq)
                    {
                        const float speed = camera.MoveSpeed
                            * (InputSystem::isKeyDown(Key::LEFTSHIFT) ? 2.0f : 1.0f);
                        targetVelocity = glm::normalize(moveDir) * speed;
                    }
                }

                const float posAlpha = SmoothAlpha(camera.PositionSmoothness, dt);
                camera.SmoothedVelocity = glm::mix(camera.SmoothedVelocity, targetVelocity, posAlpha);

                // Only dirty the transform when actually moving
                if (glm::length2(camera.SmoothedVelocity) > kDeadZoneSq)
                {
                    transform.SetTranslation(transform.Translation + camera.SmoothedVelocity * dt);
                }
            }
        }
    }

} // namespace Radis