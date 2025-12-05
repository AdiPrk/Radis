#include <PCH/pch.h>
#include "PhysicsSystem.h"
#include "ECS/ECS.h"
#include "ECS/Components/Components.h"

#include "ECS/Resources/DebugDrawResource.h"

namespace Radis
{
    void PhysicsSystem::Init()
    {
    }
    
    void PhysicsSystem::Update(float dt)
    {
        if (dt > 0.05f) dt = 0.05f;
        dt = 0.0001f;

        static bool first = true;
        if (first)
        {
            CreateSoftBodyCube();
            first = false;
        }

        m_accumulatedTime += dt;

        auto& registry = ecs->GetRegistry();
        registry.view<SoftBodyComponent>().each([&](auto entity, SoftBodyComponent& softBody)
        {
            // Integrate with RK2.
            IntegrateSoftBodyRK2(softBody, dt);

            // Draw body
            DebugDrawSoftBody(softBody);
        });
    }

    void PhysicsSystem::FrameEnd()
    {

    }

    void PhysicsSystem::CreateSoftBodyCube()
    {
        // Create entity & add soft-body component.
        Entity entity = ecs->AddEntity("SoftBodyCube");
        SoftBodyComponent& softBody = entity.AddComponent<SoftBodyComponent>();

        // Cube layout: 4 x 4 x 4 = 64 points
        const std::uint32_t nx = 4;
        const std::uint32_t ny = 4;
        const std::uint32_t nz = 4;
        const float         spacing = 0.25f;
        const float         massPerPoint = 1.0f;

        softBody.gridNx = nx;
        softBody.gridNy = ny;
        softBody.gridNz = nz;

        const std::uint32_t count = nx * ny * nz;
        softBody.particles.resize(count);

        // Center the cube in XZ, slightly above origin in Y.
        const glm::vec3 origin{
            -0.5f * spacing * (static_cast<float>(nx) - 1.0f),
             0.5f * spacing * (static_cast<float>(ny) - 1.0f),
            -0.5f * spacing * (static_cast<float>(nz) - 1.0f)
        };

        for (std::uint32_t k = 0; k < nz; ++k)
        {
            for (std::uint32_t j = 0; j < ny; ++j)
            {
                for (std::uint32_t i = 0; i < nx; ++i)
                {
                    const std::uint32_t idx = Index3D(i, j, k, nx, ny, nz);

                    glm::vec3 pos = origin + glm::vec3{
                        spacing * static_cast<float>(i),
                        spacing * static_cast<float>(j),
                        spacing * static_cast<float>(k)
                    };

                    SoftBodyParticle& p = softBody.particles[idx];
                    p.position = pos;
                    p.velocity = glm::vec3(0.0f);
                    p.inverseMass = 1.0f / massPerPoint;
                    p.isAnchor = false;
                    p.anchorPosition = pos;
                }
            }
        }

        // Helper to add a spring between two particles.
        auto addSpring = [&](std::uint32_t a, std::uint32_t b)
            {
                const glm::vec3 delta = softBody.particles[b].position - softBody.particles[a].position;

                SoftBodySpring spring;
                spring.a = a;
                spring.b = b;
                spring.restLength = glm::length(delta);
                spring.stiffness = softBody.globalStiffness;
                spring.damping = softBody.globalDamping;

                softBody.springs.push_back(spring);
            };

        // Structural + diagonal springs.
        for (std::uint32_t k = 0; k < nz; ++k)
        {
            for (std::uint32_t j = 0; j < ny; ++j)
            {
                for (std::uint32_t i = 0; i < nx; ++i)
                {
                    const std::uint32_t idx0 = Index3D(i, j, k, nx, ny, nz);

                    // Axis-aligned neighbors.
                    if (i + 1 < nx) addSpring(idx0, Index3D(i + 1, j, k, nx, ny, nz));
                    if (j + 1 < ny) addSpring(idx0, Index3D(i, j + 1, k, nx, ny, nz));
                    if (k + 1 < nz) addSpring(idx0, Index3D(i, j, k + 1, nx, ny, nz));

                    // Face diagonals.
                    if (i + 1 < nx && j + 1 < ny)
                        addSpring(idx0, Index3D(i + 1, j + 1, k, nx, ny, nz));
                    if (i + 1 < nx && k + 1 < nz)
                        addSpring(idx0, Index3D(i + 1, j, k + 1, nx, ny, nz));
                    if (j + 1 < ny && k + 1 < nz)
                        addSpring(idx0, Index3D(i, j + 1, k + 1, nx, ny, nz));

                    // Body diagonal.
                    if (i + 1 < nx && j + 1 < ny && k + 1 < nz)
                        addSpring(idx0, Index3D(i + 1, j + 1, k + 1, nx, ny, nz));
                }
            }
        }

        // Mark two top-front corners as anchors (interaction points).
        const std::uint32_t leftTopFront = Index3D(0u, ny - 1u, 0u, nx, ny, nz);
        const std::uint32_t rightTopFront = Index3D(nx - 1u, ny - 1u, 0u, nx, ny, nz);

        softBody.particles[leftTopFront].isAnchor = true;
        softBody.particles[rightTopFront].isAnchor = true;

        softBody.particles[leftTopFront].anchorPosition = softBody.particles[leftTopFront].position;
        softBody.particles[rightTopFront].anchorPosition = softBody.particles[rightTopFront].position;
    }

    void PhysicsSystem::IntegrateSoftBodyRK2(SoftBodyComponent& softBody, float dt)
    {
        const std::size_t count = softBody.particles.size();
        if (count == 0)
            return;

        EnsureTempBufferCapacity(count);

        // Aliases for brevity.
        auto& x0 = m_x0;
        auto& v0 = m_v0;
        auto& a0 = m_a0;

        auto& xMid = m_xMid;
        auto& vMid = m_vMid;
        auto& aMid = m_aMid;

        // 1) Initial state (respect anchors).
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                x0[i] = p.anchorPosition;
                v0[i] = glm::vec3(0.0f);
            }
            else
            {
                x0[i] = p.position;
                v0[i] = p.velocity;
            }
        }

        // 2) a0 = f(x0, v0)
        ComputeAccelerations(softBody, x0, v0, a0);

        // 3) Midpoint state.
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                xMid[i] = p.anchorPosition;
                vMid[i] = glm::vec3(0.0f);
            }
            else
            {
                xMid[i] = x0[i] + 0.5f * dt * v0[i];
                vMid[i] = v0[i] + 0.5f * dt * a0[i];
            }
        }

        // 4) aMid = f(xMid, vMid)
        ComputeAccelerations(softBody, xMid, vMid, aMid);

        // 5) Final update using midpoint values.
        for (std::size_t i = 0; i < count; ++i)
        {
            SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                p.position = p.anchorPosition;
                p.velocity = glm::vec3(0.0f);
                continue;
            }

            p.velocity = v0[i] + dt * aMid[i];
            p.position = x0[i] + dt * vMid[i];

            const float velocityDamping = 0.2f;
            p.velocity = (v0[i] + dt * aMid[i]) * (1.0f - velocityDamping * dt);
        }
    }

    void PhysicsSystem::ComputeAccelerations(const SoftBodyComponent& softBody, const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& velocities, std::vector<glm::vec3>& outAccelerations)
    {
        const std::size_t count = softBody.particles.size();
        outAccelerations.assign(count, glm::vec3(0.0f));

        // Gravity.
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];
            if (!p.isAnchor && p.inverseMass > 0.0f)
            {
                outAccelerations[i] += -0.1f;/*softBody.gravity;*/
            }
        }

        // Springs.
        for (const SoftBodySpring& spring : softBody.springs)
        {
            const std::uint32_t ia = spring.a;
            const std::uint32_t ib = spring.b;

            const glm::vec3& xa = positions[ia];
            const glm::vec3& xb = positions[ib];
            const glm::vec3& va = velocities[ia];
            const glm::vec3& vb = velocities[ib];

            glm::vec3 delta = xb - xa;
            const float length = glm::length(delta);
            if (length < 1e-6f)
                continue;

            const glm::vec3 dir = delta / length;

            const float stretch = length - spring.restLength;
            const float relativeSpeed = glm::dot(vb - va, dir);

            const glm::vec3 force =
                -spring.stiffness * stretch * dir
                - spring.damping * relativeSpeed * dir;

            SoftBodyParticle const& pa = softBody.particles[ia];
            SoftBodyParticle const& pb = softBody.particles[ib];

            if (!pa.isAnchor && pa.inverseMass > 0.0f)
            {
                outAccelerations[ia] += force * pa.inverseMass;
            }

            if (!pb.isAnchor && pb.inverseMass > 0.0f)
            {
                outAccelerations[ib] -= force * pb.inverseMass;
            }
        }

        // Anchors: explicitly zero acceleration.
        for (std::size_t i = 0; i < count; ++i)
        {
            if (softBody.particles[i].isAnchor)
            {
                outAccelerations[i] = glm::vec3(0.0f);
            }
        }
    }

    void PhysicsSystem::EnsureTempBufferCapacity(std::size_t count)
    {
        if (m_x0.size() < count)
        {
            m_x0.resize(count);
            m_v0.resize(count);
            m_a0.resize(count);

            m_xMid.resize(count);
            m_vMid.resize(count);
            m_aMid.resize(count);
        }
    }

    void PhysicsSystem::DebugDrawSoftBody(const SoftBodyComponent& softBody)
    {
        // Colors
        const glm::vec4 springColor = glm::vec4(0.2f, 0.7f, 1.0f, 1.0f); // cyan-ish
        const glm::vec4 particleColor = glm::vec4(1.0f, 0.9f, 0.6f, 1.0f); // warm
        const glm::vec4 anchorColor = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); // red
        const float     particleSize = 0.02f;
        const float     anchorSize = 0.03f;
        const float     springThickness = 0.003f;

        const auto& particles = softBody.particles;
        const auto& springs = softBody.springs;

        // Draw springs as lines between particles
        for (const SoftBodySpring& s : springs)
        {
            if (s.a >= particles.size() || s.b >= particles.size())
                continue;

            const glm::vec3& pa = particles[s.a].position;
            const glm::vec3& pb = particles[s.b].position;

            DebugDrawResource::DrawLine(pa, pb, springColor, springThickness);
        }

        // Draw particles as small cubes (anchors in a different color)
        for (const SoftBodyParticle& p : particles)
        {
            const glm::vec3& center = p.position;
            const bool isAnchor = p.isAnchor;

            const glm::vec3 size = glm::vec3(isAnchor ? anchorSize : particleSize);
            const glm::vec4 color = isAnchor ? anchorColor : particleColor;

            DebugDrawResource::DrawCube(center, size, color);
        }

    }
}
