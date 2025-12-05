#include <PCH/pch.h>
#include "CameraSystem.h"
#include "InputSystem.h"

#include "ECS/ECS.h"
#include "ECS/Components/Components.h"

namespace Radis
{
    static float AngleDiffDeg(float from, float to) {
        // returns shortest signed difference in degrees (to - from) in range [-180,180)
        float diff = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
        return diff;
    }

    static float LerpAngleDeg(float a, float b, float t) {
        float d = AngleDiffDeg(a, b);
        return a + d * t;
    }

    void CameraSystem::Update(float dt)
    {
        if (dt <= 0.0f) return;

        entt::registry& registry = ecs->GetRegistry();
        auto view = registry.view<TransformComponent, CameraComponent>();

        constexpr float MAX_PITCH_DEG = 89.9f; // avoid singularity exactly at 90
        const glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);

        for (auto entityHandle : view)
        {
            Entity entity(&registry, entityHandle);
            auto& transform = entity.GetComponent<TransformComponent>();
            auto& camera = entity.GetComponent<CameraComponent>();

            // --- initialize once ---
            if (!camera.isInitialized)
            {
                camera.TargetYaw = transform.Rotation.y;
                camera.TargetPitch = transform.Rotation.x;
                camera.SmoothedYaw = camera.TargetYaw;
                camera.SmoothedPitch = camera.TargetPitch;
                camera.SmoothedPosition = transform.Translation;
                camera.SmoothedMouseDX = 0.0f;
                camera.SmoothedMouseDY = 0.0f;
                camera.isInitialized = true;
            }

            const bool mouseHeld = InputSystem::isMouseDown(Mouse::RIGHT) && !InputSystem::IsMouseInputLocked();

            // --- gather raw mouse delta ---
            float rawDX = 0.0f;
            float rawDY = 0.0f;
            if (mouseHeld) {
                rawDX = InputSystem::GetMouseDeltaX();
                rawDY = InputSystem::GetMouseDeltaY();
            }

            // --- smooth raw mouse delta (exponential smoothing, frame-rate independent) ---
            // smoothing factor: alpha = 1 - exp(-k * dt)
            const float mouseK = camera.MouseSmoothness; // larger -> less smoothing (faster)
            float mouseAlpha = 1.0f - std::exp(-mouseK * dt);
            camera.SmoothedMouseDX = glm::mix(camera.SmoothedMouseDX, rawDX, mouseAlpha);
            camera.SmoothedMouseDY = glm::mix(camera.SmoothedMouseDY, rawDY, mouseAlpha);

            // --- update target yaw/pitch using smoothed mouse delta (degrees) ---
            if (mouseHeld)
            {
                float dYaw = camera.SmoothedMouseDX * camera.MouseSensitivity;
                float dPitch = camera.SmoothedMouseDY * camera.MouseSensitivity * (camera.InvertY ? -1.0f : 1.0f);

                camera.TargetYaw += dYaw;
                camera.TargetPitch += dPitch;

                // clamp pitch on target
                camera.TargetPitch = glm::clamp(camera.TargetPitch, -MAX_PITCH_DEG, MAX_PITCH_DEG);

                // wrap target yaw to keep numbers small
                if (camera.TargetYaw > 360.0f || camera.TargetYaw < -360.0f)
                    camera.TargetYaw = std::fmod(camera.TargetYaw, 360.0f);
            }
            else
            {
                // If not holding, keep target in sync with transform (so external changes are respected)
                camera.TargetYaw = transform.Rotation.y;
                camera.TargetPitch = transform.Rotation.x;
            }

            // --- smooth rotation: exponential smoothing toward target (frame-rate independent) ---
            const float rotK = camera.RotationSmoothness; // tune: larger -> faster (less smooth)
            float rotAlpha = 1.0f - std::exp(-rotK * dt);

            camera.SmoothedYaw = LerpAngleDeg(camera.SmoothedYaw, camera.TargetYaw, rotAlpha);
            // pitch is within [-90,90], linear interpolation is fine
            camera.SmoothedPitch = glm::mix(camera.SmoothedPitch, camera.TargetPitch, rotAlpha);

            // write smoothed rotation back to transform (in degrees)
            transform.Rotation.x = camera.SmoothedPitch;
            transform.Rotation.y = camera.SmoothedYaw;
            transform.Rotation.z = 0.0f;

            // --- compute direction vectors from smoothed Euler angles (degrees -> radians) ---
            {
                const float yawRad = glm::radians(camera.SmoothedYaw);
                const float pitchRad = glm::radians(camera.SmoothedPitch);

                glm::vec3 forward;
                forward.x = std::sin(yawRad) * std::cos(pitchRad);
                forward.y = std::sin(pitchRad);
                forward.z = -std::cos(yawRad) * std::cos(pitchRad);

                forward = glm::normalize(forward);

                glm::vec3 right = glm::cross(forward, WORLD_UP);
                if (glm::length2(right) < 1e-6f)
                {
                    right = glm::vec3(1.0f, 0.0f, 0.0f);
                }
                else
                {
                    right = glm::normalize(right);
                }

                glm::vec3 up = glm::normalize(glm::cross(right, forward));

                camera.Forward = forward;
                camera.Up = up;
            }

            // --- MOVEMENT (same inputs) --- 
            glm::vec3 moveDir(0.0f);
            if (mouseHeld)
            {
                glm::vec3 forwardDir = glm::normalize(camera.Forward);
                glm::vec3 rightDir = glm::normalize(glm::cross(forwardDir, camera.Up));

                if (InputSystem::isKeyDown(Key::W)) moveDir += forwardDir;
                if (InputSystem::isKeyDown(Key::S)) moveDir -= forwardDir;
                if (InputSystem::isKeyDown(Key::A)) moveDir -= rightDir;
                if (InputSystem::isKeyDown(Key::D)) moveDir += rightDir;

                if (InputSystem::isKeyDown(Key::E)) moveDir += WORLD_UP;
                if (InputSystem::isKeyDown(Key::Q)) moveDir -= WORLD_UP;
            }

            if (glm::length2(moveDir) > std::numeric_limits<float>::epsilon())
            {
                const float baseSpeed = camera.MoveSpeed;
                const float speedMultiplier = InputSystem::isKeyDown(Key::LEFTSHIFT) ? 2.0f : 1.0f;
                const float currentSpeed = baseSpeed * speedMultiplier;

                // desired new position if movement is applied this frame (instant desired)
                glm::vec3 desiredPos = transform.Translation + glm::normalize(moveDir) * currentSpeed * dt;

                // Smooth Position toward desiredPos
                const float posK = camera.PositionSmoothness; // larger -> faster (less smooth)
                float posAlpha = 1.0f - std::exp(-posK * dt);
                camera.SmoothedPosition = glm::mix(camera.SmoothedPosition, desiredPos, posAlpha);

                // write smoothed position back to transform
                transform.Translation = camera.SmoothedPosition;
            }
            else
            {
                // If no movement input, gently damp toward current transform target to avoid one-frame jumps
                const float posK = camera.PositionSmoothness;
                float posAlpha = 1.0f - std::exp(-posK * dt);
                camera.SmoothedPosition = glm::mix(camera.SmoothedPosition, transform.Translation, posAlpha);
                transform.Translation = camera.SmoothedPosition;
            }
        }
    }
}
