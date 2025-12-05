#pragma once

#include "Graphics/Common/Animation/AnimationLibrary.h" // For AnimationLibrary::INVALID_ANIMATION_INDEX

namespace Radis {

	struct TagComponent
	{
		std::string Tag;
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		glm::mat4 GetTransform() const
		{
			// SRT - Translate * Rotate * Scale
			return glm::translate(glm::mat4(1.0f), Translation) *
				   glm::toMat4(glm::quat(Rotation)) *
				   glm::scale(glm::mat4(1.0f), Scale);
		}

		glm::mat4 mat4() const;
		glm::mat3 normalMatrix() const;
	};

	struct ModelComponent
	{
		std::string ModelPath = "";
        glm::vec4 tintColor = glm::vec4(1.0f);

        bool useMetallicOverride = false;
        bool useRoughnessOverride = false;
		float metallicOverride = 1.0f;
        float roughnessOverride = 1.0f;
	};

	struct AnimationComponent
	{
        bool IsPlaying = true;
		uint32_t AnimationIndex = AnimationLibrary::INVALID_ANIMATION_INDEX;
        float AnimationTime = 0.0f;
		bool inPlace = false;

		// Used internally:
        float PrevAnimationTime = AnimationTime;
        bool prevInPlace = inPlace;
        uint32_t BoneOffset = 0;
	};

	struct CameraComponent
	{
		float FOV = 45.0f;
		float Near = 0.1f;
		float Far = 1000.0f;

        glm::vec3 Forward = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);

		float Yaw{ 0.0f };
		float Pitch{ 0.0f };
		float MouseSensitivity{ 0.15f };
		bool InvertY{ true };
		float MoveSpeed{ 10.f };

		// --- Internal ---
		// in CameraComponent
		float TargetYaw = 0.0f;
		float TargetPitch = 0.0f;

		// Smoothed current values (what we write to transform and use for lookAt)
		float SmoothedYaw = 0.0f;
		float SmoothedPitch = 0.0f;
		glm::vec3 SmoothedPosition = glm::vec3(0.0f);

		// Mouse smoothing buffer
		float SmoothedMouseDX = 0.0f;
		float SmoothedMouseDY = 0.0f;

		// Tweakable smoothing params (tune these)
		float MouseSmoothness = 20.0f; // higher -> less smoothing of raw mouse (more snappy)
		float RotationSmoothness = 18.0f; // higher -> faster rotation follow (less smoothing)
		float PositionSmoothness = 12.0f; // higher -> faster position follow (less smoothing)
		bool isInitialized{ false };
	};

	struct LightComponent 
	{
		enum LightType
		{
			DIRECTIONAL = 0,
			POINT = 1,
			SPOT = 2
		};
	
		glm::vec3 Position{};
		float Radius{ 1.f };        // For point/spot attenuation
		glm::vec3 Color{ 1.f, 1.f, 1.f };
		float Intensity{ 1.f };
		glm::vec3 Direction{ 0.f, 0.f, -1.f };
		float InnerCone{ glm::radians(30.f) }; // for spot
		float OuterCone{ glm::radians(60.f) }; // for spot
		LightType Type{ POINT };
	};

	struct SoftBodyParticle
	{
		glm::vec3 position{ 0.0f };
		glm::vec3 velocity{ 0.0f };

		/// Inverse mass (0.0f => infinite mass / static).
		float inverseMass = 1.0f;

		bool isAnchor = false;
		glm::vec3 anchorPosition{ 0.0f };
	};

	struct SoftBodySpring
	{
		std::uint32_t a = 0; // Index of first particle
		std::uint32_t b = 0; // Index of second particle

		float restLength = 0.0f;
		float stiffness = 0.0f; // Hooke stiffness (k)
		float damping = 0.0f; // Spring damping (c)
	};

	struct SoftBodyComponent
	{
		std::vector<SoftBodyParticle> particles;
		std::vector<SoftBodySpring>   springs;

		// Global settings (can be overridden per-entity if you want).
		glm::vec3 gravity{ 0.0f, 0.0f, 0.0f };

		float globalStiffness = 50.0f;
		float globalDamping = 10.0f;

		// If you want to know the logical layout (for anchors/interaction).
		std::uint32_t gridNx = 0;
		std::uint32_t gridNy = 0;
		std::uint32_t gridNz = 0;
	};
}
