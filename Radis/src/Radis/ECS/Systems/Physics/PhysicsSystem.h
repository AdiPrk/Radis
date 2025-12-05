#pragma once

#include "../ISystem.h"

namespace Radis
{
    struct SoftBodyComponent;

    class PhysicsSystem : public ISystem
    {
    public:
        PhysicsSystem() : ISystem("PhysicsSystem") {};
        ~PhysicsSystem() {}

        void Init() override;
        void Update(float dt) override;
        void FrameEnd() override;

        // Creation helpers ----------------------------------------------------
        void CreateSoftBodyCube();

        // Integration / force computation ------------------------------------
        void IntegrateSoftBodyRK2(SoftBodyComponent& softBody, float dt);

        void ComputeAccelerations(
            const SoftBodyComponent& softBody,
            const std::vector<glm::vec3>& positions,
            const std::vector<glm::vec3>& velocities,
            std::vector<glm::vec3>& outAccelerations);

        // Utilities -----------------------------------------------------------
        static std::uint32_t Index3D(std::uint32_t i, std::uint32_t j, std::uint32_t k, std::uint32_t nx, std::uint32_t ny, std::uint32_t nz)
        {
            return i + nx * (j + ny * k);
        }

        void EnsureTempBufferCapacity(std::size_t count);

    private:
        void DebugDrawSoftBody(const SoftBodyComponent& softBody);

        // Used to animate anchor points for environment interaction.
        float m_accumulatedTime = 0.0f;

        // Reused temporary buffers (mutable because Update is logically const w.r.t. these).
        std::vector<glm::vec3> m_x0;
        std::vector<glm::vec3> m_v0;
        std::vector<glm::vec3> m_a0;

        std::vector<glm::vec3> m_xMid;
        std::vector<glm::vec3> m_vMid;
        std::vector<glm::vec3> m_aMid;
    };
}