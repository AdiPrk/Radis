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
        // Clamp dt for stability.
        if (dt > 0.05f)
            dt = 0.05f;

        static bool first = true;
        if (first)
        {
            CreateSoftBodyCube();
            first = false;
        }

        mAccumulatedTime += dt;

        auto& registry = ecs->GetRegistry();
        registry.view<SoftBodyComponent>().each([&](auto entity, SoftBodyComponent& softBody)
        {
            IntegrateSoftBodyRK2(softBody, dt);
            DebugDrawSoftBodyFancy(softBody);
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

        // Physics params.
        softBody.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        softBody.globalStiffness = 80.0f;
        softBody.globalDamping = 8.0f;

        // Particles + springs.
        InitializeSoftBodyParticles(softBody, 4, 4, 4, 0.25f, 1.0f);
        BuildSoftBodySprings(softBody);
    }

    void PhysicsSystem::InitializeSoftBodyParticles(SoftBodyComponent& softBody, uint32_t nx, uint32_t ny, uint32_t nz, float spacing, float massPerPoint)
    {
        softBody.gridNx = nx;
        softBody.gridNy = ny;
        softBody.gridNz = nz;

        softBody.particles.clear();
        softBody.particles.resize(nx * ny * nz);

        // Center the cube in XZ, slightly above origin in Y.
        const glm::vec3 origin{
            -0.5f * spacing * (static_cast<float>(nx) - 1.0f),
             0.5f * spacing * (static_cast<float>(ny) - 1.0f),
            -0.5f * spacing * (static_cast<float>(nz) - 1.0f)
        };

        float invMass = 1.0f / massPerPoint;

        for (uint32_t k = 0; k < nz; ++k)
        for (uint32_t j = 0; j < ny; ++j)
        for (uint32_t i = 0; i < nx; ++i)
        {
            uint32_t idx = Index3D(i, j, k, nx, ny, nz);

            glm::vec3 pos = origin + spacing * glm::vec3(
                static_cast<float>(i),
                static_cast<float>(j),
                static_cast<float>(k)
            );

            bool isTopFace = (j == ny - 1u);

            SoftBodyParticle& p = softBody.particles[idx];
            p.position = pos;
            p.anchorPosition = pos;
            p.velocity = glm::vec3(0.0f);
            p.isAnchor = isTopFace;
            p.inverseMass = isTopFace ? 0.0f : invMass;
        }
    }

    void PhysicsSystem::BuildSoftBodySprings(SoftBodyComponent& softBody)
    {
        const uint32_t nx = softBody.gridNx;
        const uint32_t ny = softBody.gridNy;
        const uint32_t nz = softBody.gridNz;

        softBody.springs.clear();
        softBody.springs.reserve(300); // reduce reallocs

        auto& particles = softBody.particles;

        auto addSpring = [&](uint32_t a, uint32_t b)
        {
            SoftBodySpring spring{};
            spring.a = a;
            spring.b = b;

            const glm::vec3 delta =
                particles[b].position - particles[a].position;

            spring.restLength = glm::length(delta);
            spring.stiffness = softBody.globalStiffness;
            spring.damping = softBody.globalDamping;

            softBody.springs.push_back(spring);
        };

        auto index = [&](uint32_t i, uint32_t j, uint32_t k)
        {
            return Index3D(i, j, k, nx, ny, nz);
        };

        for (uint32_t k = 0; k < nz; ++k)
        {
            for (uint32_t j = 0; j < ny; ++j)
            {
                for (uint32_t i = 0; i < nx; ++i)
                {
                    const uint32_t idx000 = index(i, j, k);

                    // -----------------------------
                    // Structural
                    // -----------------------------
                    if (i + 1 < nx) addSpring(idx000, index(i + 1, j, k)); // +X
                    if (j + 1 < ny) addSpring(idx000, index(i, j + 1, k)); // +Y
                    if (k + 1 < nz) addSpring(idx000, index(i, j, k + 1)); // +Z

                    // -----------------------------
                    // Face diagonals
                    // -----------------------------
                    // XY faces (constant k)
                    if (i + 1 < nx && j + 1 < ny)
                    {
                        const uint32_t idx100 = index(i + 1, j, k);
                        const uint32_t idx010 = index(i, j + 1, k);
                        const uint32_t idx110 = index(i + 1, j + 1, k);

                        addSpring(idx000, idx110); // (i,j,k)   <-> (i+1,j+1,k)
                        addSpring(idx100, idx010); // (i+1,j,k) <-> (i,  j+1,k)
                    }

                    // XZ faces (constant j)
                    if (i + 1 < nx && k + 1 < nz)
                    {
                        const uint32_t idx100 = index(i + 1, j, k);
                        const uint32_t idx001 = index(i, j, k + 1);
                        const uint32_t idx101 = index(i + 1, j, k + 1);

                        addSpring(idx000, idx101); // (i,j,k)   <-> (i+1,j,  k+1)
                        addSpring(idx100, idx001); // (i+1,j,k) <-> (i,  j,  k+1)
                    }

                    // YZ faces (constant i)
                    if (j + 1 < ny && k + 1 < nz)
                    {
                        const uint32_t idx010 = index(i, j + 1, k);
                        const uint32_t idx001 = index(i, j, k + 1);
                        const uint32_t idx011 = index(i, j + 1, k + 1);

                        addSpring(idx000, idx011); // (i,j,k)   <-> (i,  j+1,k+1)
                        addSpring(idx010, idx001); // (i,j+1,k) <-> (i,  j,  k+1)
                    }

                    // -----------------------------
                    // Body diagonals
                    // -----------------------------
                    if (i + 1 < nx && j + 1 < ny && k + 1 < nz)
                    {
                        const uint32_t idx100 = index(i + 1, j, k);
                        const uint32_t idx010 = index(i, j + 1, k);
                        const uint32_t idx110 = index(i + 1, j + 1, k);
                        const uint32_t idx001 = index(i, j, k + 1);
                        const uint32_t idx101 = index(i + 1, j, k + 1);
                        const uint32_t idx011 = index(i, j + 1, k + 1);
                        const uint32_t idx111 = index(i + 1, j + 1, k + 1);

                        // 4 space diagonals of the 2x2x2 cell
                        addSpring(idx000, idx111); // 000 <-> 111
                        addSpring(idx100, idx011); // 100 <-> 011
                        addSpring(idx010, idx101); // 010 <-> 101
                        addSpring(idx110, idx001); // 110 <-> 001
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Integration / forces
    // -------------------------------------------------------------------------

    void PhysicsSystem::IntegrateSoftBodyRK2(SoftBodyComponent& softBody, float dt)
    {
        const std::size_t count = softBody.particles.size();
        if (count == 0) return;

        EnsureBufferCapacity(count);

        // 1) Initial state (respect anchors).
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                mX0[i] = p.anchorPosition;
                mV0[i] = glm::vec3(0.0f);
            }
            else
            {
                mX0[i] = p.position;
                mV0[i] = p.velocity;
            }
        }

        // 2) mA0 = f(mX0, mV0)
        ComputeAccelerations(softBody, mX0, mV0, mA0);

        // 3) Midpoint state.
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                mXMid[i] = p.anchorPosition;
                mVMid[i] = glm::vec3(0.0f);
            }
            else
            {
                mXMid[i] = mX0[i] + 0.5f * dt * mV0[i];
                mVMid[i] = mV0[i] + 0.5f * dt * mA0[i];
            }
        }

        // 4) mAMid = f(mXMid, mVMid)
        ComputeAccelerations(softBody, mXMid, mVMid, mAMid);

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

            p.velocity = mV0[i] + dt * mAMid[i];

            float velocityDamping = 0.0f; // Not damping for now
            p.velocity *= (1.0f - velocityDamping * dt);

            p.position = mX0[i] + dt * mVMid[i];
        }
    }

    void PhysicsSystem::ComputeAccelerations(const SoftBodyComponent& softBody, const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& velocities, std::vector<glm::vec3>& outAccelerations)
    {
        const std::size_t count = softBody.particles.size();
        outAccelerations.assign(count, glm::vec3(0.0f));

        // Gravity
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];
            if (!p.isAnchor && p.inverseMass > 0.0f)
            {
                outAccelerations[i] += softBody.gravity;
            }
        }

        // Springs
        for (const SoftBodySpring& spring : softBody.springs)
        {
            const uint32_t ia = spring.a;
            const uint32_t ib = spring.b;

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

            // Force on particle b
            const glm::vec3 force =
                -spring.stiffness * stretch * dir
                - spring.damping * relativeSpeed * dir;

            const SoftBodyParticle& pa = softBody.particles[ia];
            const SoftBodyParticle& pb = softBody.particles[ib];

            // Apply to b
            if (!pb.isAnchor && pb.inverseMass > 0.0f)
            {
                outAccelerations[ib] += force * pb.inverseMass;
            }

            // Equal and opposite on a
            if (!pa.isAnchor && pa.inverseMass > 0.0f)
            {
                outAccelerations[ia] -= force * pa.inverseMass;
            }
        }
    }

    uint32_t PhysicsSystem::Index3D(uint32_t i, uint32_t j, uint32_t k, uint32_t nx, uint32_t ny, uint32_t nz)
    {
        return i + nx * (j + ny * k);
    }

    void PhysicsSystem::EnsureBufferCapacity(std::size_t count)
    {
        if (mX0.size() >= count) return;

        mX0.resize(count);
        mV0.resize(count);
        mA0.resize(count);
        mXMid.resize(count);
        mVMid.resize(count);
        mAMid.resize(count);
    }

    void PhysicsSystem::DebugDrawSoftBody(const SoftBodyComponent& softBody)
    {
        glm::vec4 springColor = glm::vec4(0.2f, 0.7f, 1.0f, 1.0f);
        glm::vec4 particleColor = glm::vec4(1.0f, 0.9f, 0.6f, 1.0f);
        glm::vec4 anchorColor = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);

        float particleSize = 0.02f;
        float anchorSize = 0.03f;
        float springThickness = 0.003f;

        const auto& ps = softBody.particles;
        const auto& ss = softBody.springs;

        // Springs as lines.
        for (const SoftBodySpring& s : ss)
        {
            if (s.a >= ps.size() || s.b >= ps.size()) continue;
            DebugDrawResource::DrawLine(ps[s.a].position, ps[s.b].position, springColor, springThickness);
        }

        // Particles as cubes (anchors in different color).
        for (const SoftBodyParticle& p : ps)
        {
            glm::vec3 size = glm::vec3(p.isAnchor ? anchorSize : particleSize);
            glm::vec4 color = p.isAnchor ? anchorColor : particleColor;

            DebugDrawResource::DrawCube(p.position, size, color);
        }
    }

    void PhysicsSystem::DebugDrawSoftBodyFancy(const SoftBodyComponent& softBody)
    {
        const auto& particles = softBody.particles;
        const auto& springs = softBody.springs;

        if (particles.empty())
            return;

        // ---------------------------------------------------------------------
        // Tuning parameters
        // ---------------------------------------------------------------------

        // Springs
        glm::vec4 springNeutralColor = glm::vec4(0.25f, 0.75f, 1.0f, 1.0f);  // base cyan
        glm::vec4 springCompressedColor = glm::vec4(0.30f, 0.90f, 0.60f, 1.0f); // green-ish
        glm::vec4 springStretchedColor = glm::vec4(1.00f, 0.35f, 0.35f, 1.0f); // red

        float baseSpringThickness = 0.0025f;
        float maxVisualStrain = 0.5f; // 50% compression/stretch -> full effect

        // Particles
        glm::vec4 particleBaseColor = glm::vec4(1.0f, 0.92f, 0.70f, 1.0f);  // warm sand
        glm::vec4 particleHotColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // energetic

        glm::vec4 anchorColor = glm::vec4(1.0f, 0.25f, 0.25f, 1.0f);  // bright red

        float particleSize = 0.02f;
        float anchorSize = 0.03f;

        // Outline
        float outlineScale = 1.25f;
        float outlineAlpha = 0.35f;

        // ---------------------------------------------------------------------
        // Pre-pass: compute max speed for color normalization
        // ---------------------------------------------------------------------
        float maxSpeed = 0.0f;
        for (const SoftBodyParticle& p : particles)
        {
            if (p.isAnchor)
                continue;

            float speed = glm::length(p.velocity);
            if (speed > maxSpeed)
                maxSpeed = speed;
        }
        if (maxSpeed < 1e-4f)
            maxSpeed = 1e-4f; // avoid division by zero

        // ---------------------------------------------------------------------
        // Springs: color & thickness based on strain
        // ---------------------------------------------------------------------
        for (const SoftBodySpring& s : springs)
        {
            if (s.a >= particles.size() || s.b >= particles.size())
                continue;

            const glm::vec3& pa = particles[s.a].position;
            const glm::vec3& pb = particles[s.b].position;

            glm::vec3 delta = pb - pa;
            float currentLen = glm::length(delta);
            if (currentLen < 1e-6f || s.restLength < 1e-6f)
            {
                DebugDrawResource::DrawLine(pa, pb, springNeutralColor, baseSpringThickness);
                continue;
            }

            float strain = (currentLen - s.restLength) / s.restLength; // <0 compressed, >0 stretched
            float absStrain = glm::abs(strain);

            float t = glm::clamp(absStrain / maxVisualStrain, 0.0f, 1.0f);

            glm::vec4 springColor;
            if (strain >= 0.0f)
                springColor = glm::mix(springNeutralColor, springStretchedColor, t);
            else
                springColor = glm::mix(springNeutralColor, springCompressedColor, t);

            float thickness = baseSpringThickness * (1.0f + 1.5f * t);

            DebugDrawResource::DrawLine(pa, pb, springColor, thickness);
        }

        // ---------------------------------------------------------------------
        // Particles: constant size; color based on speed; anchors distinct
        // ---------------------------------------------------------------------
        for (const SoftBodyParticle& p : particles)
        {
            bool isAnchor = p.isAnchor;

            float speed = glm::length(p.velocity);
            float speedNorm = glm::clamp(speed / maxSpeed, 0.0f, 1.0f);

            glm::vec3 size;
            glm::vec4 color;

            if (isAnchor)
            {
                size = glm::vec3(anchorSize);
                color = anchorColor;
            }
            else
            {
                size = glm::vec3(particleSize);
                color = glm::mix(particleBaseColor, particleHotColor, speedNorm);
            }

            // Static outline for readability
            glm::vec3 outlineSize = size * outlineScale;
            glm::vec4 outlineColor = glm::vec4(0.0f, 0.0f, 0.0f, outlineAlpha);

            DebugDrawResource::DrawCube(p.position, outlineSize, outlineColor);
            DebugDrawResource::DrawCube(p.position, size, color);
        }
    }
}
