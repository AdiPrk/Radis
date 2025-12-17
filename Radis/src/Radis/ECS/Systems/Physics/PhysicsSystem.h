#pragma once

#include "../ISystem.h"

namespace Radis
{
    struct SoftBodyComponent;
    struct TransformComponent;

    class PhysicsSystem : public ISystem
    {
    public:
        PhysicsSystem() : ISystem("PhysicsSystem") {}
        ~PhysicsSystem() {}

        void Init() override;
        void Update(float dt) override;
        void FrameEnd() override;

        void SyncSoftBodyWithTransform(SoftBodyComponent& softBody, TransformComponent& transform);

        // Creation helpers ----------------------------------------------------
        void CreateSoftBodyCube(int n, glm::vec3 position = glm::vec3(0.0f), std::string debugName = "");

        // Integration / force computation ------------------------------------
        void IntegrateSoftBodyRK4(SoftBodyComponent& softBody, float dt);
        void ComputeAccelerations(const SoftBodyComponent& softBody, const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& velocities, std::vector<glm::vec3>& outAccelerations);

        // Utility functions ---------------------------------------------------
        void EnsureBufferCapacity(std::size_t count);
        static uint32_t Index3D(uint32_t i, uint32_t j, uint32_t k, uint32_t nx, uint32_t ny, uint32_t nz);

    private:
        void InitializeSoftBodyParticles(SoftBodyComponent& softBody, uint32_t nx, uint32_t ny, uint32_t nz, float spacing, float massPerPoint);
        void BuildSoftBodySprings(SoftBodyComponent& softBody);
        void DebugDrawSoftBodyFancy(const SoftBodyComponent& softBody);

        // Used to animate anchor points for environment interaction.
        float mAccumulatedTime = 0.0f;

        // Reused buffers for integration.
        std::vector<glm::vec3> mX0;
        std::vector<glm::vec3> mV0;
        std::vector<glm::vec3> mA0;

        std::vector<glm::vec3> mXMid;
        std::vector<glm::vec3> mVMid;
        std::vector<glm::vec3> mAMid;

        std::vector<glm::vec3> mK1x, mK2x, mK3x, mK4x;
        std::vector<glm::vec3> mK1v, mK2v, mK3v, mK4v;
    };
}
