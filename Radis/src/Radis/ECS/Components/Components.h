/*****************************************************************//**
 * \file   Components.h
 * \brief  Core ECS component definitions
 * 
 * \author Aditya Prakash
 * \date   2026
 *********************************************************************/
#pragma once

#include "Graphics/Common/Animation/AnimationLibrary.h" // For AnimationLibrary::INVALID_ANIMATION_INDEX

namespace Radis
{
    // =========================================================================
    // Core Components
    // =========================================================================

    struct TagComponent
    {
        std::string Tag;
    };

    struct TransformComponent
    {
        glm::vec3 Translation = glm::vec3(0.0f);
        glm::vec3 Rotation = glm::vec3(0.0f);
        glm::vec3 Scale = glm::vec3(1.0f);

        glm::mat4 GetTransform();
        glm::mat3 GetNormalMatrix() const;

        void SetTranslation(float x, float y, float z);
        void SetTranslation(const glm::vec3& tr);
        void SetRotation(float x, float y, float z);
        void SetRotation(const glm::vec3& rot);
        void SetScale(float x, float y, float z);
        void SetScale(const glm::vec3& scale);
        void SetScale(float uniformScale);

        glm::mat4 mCachedTransform = glm::mat4(1.0f);
        bool mIsDirty = true;
    };

    // =========================================================================
    // Rendering Components
    // =========================================================================

    struct ModelComponent
    {
        std::string ModelPath;
        uint32_t ModelID = 0;
        bool UpdateModelID = true;
        glm::vec4 TintColor = glm::vec4(1.0f);

        bool NormalizeModel = true;
        bool UseMetallicOverride = false;
        bool UseRoughnessOverride = false;
        bool UseEmissiveOverride = false;
        float MetallicOverride = 1.0f;
        float RoughnessOverride = 1.0f;
        glm::vec4 EmissiveOverride = glm::vec4(0.0f);
    };

    struct AnimationComponent
    {
        bool IsPlaying = true;
        uint32_t AnimationIndex = AnimationLibrary::INVALID_ANIMATION_INDEX;
        float AnimationTime = 0.0f;
        bool InPlace = false;

        // Internal state
        float PrevAnimationTime = 0.0f;
        bool PrevInPlace = false;
        uint32_t BoneOffset = 0;
    };

    struct CameraComponent
    {
        float FOV = 45.0f;
        float Near = 0.1f;
        float Far = 1000.0f;

        glm::vec3 Forward = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);

        float Yaw = 0.0f;
        float Pitch = 0.0f;
        float MouseSensitivity = 0.15f;
        bool InvertY = true;
        float MoveSpeed = 10.0f;

        // Internal smoothing state
        float TargetYaw = 0.0f;
        float TargetPitch = 0.0f;
        float SmoothedYaw = 0.0f;
        float SmoothedPitch = 0.0f;
        glm::vec3 SmoothedPosition = glm::vec3(0.0f);
        float SmoothedMouseDX = 0.0f;
        float SmoothedMouseDY = 0.0f;

        // Smoothing parameters
        float MouseSmoothness = 20.0f;
        float RotationSmoothness = 18.0f;
        float PositionSmoothness = 12.0f;
        bool IsInitialized = false;
    };

    // =========================================================================
    // Lighting Components
    // =========================================================================

    struct LightComponent
    {
        enum class Types : uint32_t
        {
            Directional = 0,
            Point = 1,
            Spot = 2
        };

        glm::vec3 Position = glm::vec3(0.0f);
        float Radius = 1.0f;
        glm::vec3 Color = glm::vec3(1.0f);
        float Intensity = 1.0f;
        glm::vec3 Direction = glm::vec3(0.0f, 0.0f, -1.0f);
        float InnerCone = glm::radians(30.0f);
        float OuterCone = glm::radians(60.0f);
        Types LightType = Types::Point;
    };

    // =========================================================================
    // Physics Components
    // =========================================================================

    struct SoftBodyParticle
    {
        glm::vec3 Position = glm::vec3(0.0f);
        glm::vec3 Velocity = glm::vec3(0.0f);
        float InverseMass = 1.0f;
        bool IsAnchor = false;
        glm::vec3 AnchorPosition = glm::vec3(0.0f);
    };

    struct SoftBodySpring
    {
        uint32_t IndexA = 0;
        uint32_t IndexB = 0;
        float RestLength = 0.0f;
        float Stiffness = 0.0f;
        float Damping = 0.0f;
    };

    struct SoftBodyComponent
    {
        std::vector<SoftBodyParticle> Particles;
        std::vector<SoftBodySpring> Springs;

        glm::vec3 Gravity = glm::vec3(0.0f);
        glm::vec3 GlobalOffset = glm::vec3(0.0f);
        float GlobalStiffness = 50.0f;
        float GlobalDamping = 10.0f;

        uint32_t GridNx = 0;
        uint32_t GridNy = 0;
        uint32_t GridNz = 0;

        glm::vec3 LastTransformPosition = glm::vec3(0.0f);
    };

} // namespace Radis
