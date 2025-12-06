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
        if (dt > 0.1f) dt = 0.1f;

        static bool first = true;
        if (first)
        {
            float cursorX = 0.0f;
            int maxN = 8;
            for (int i = 0; i < maxN - 1; ++i)
            {
                int n = 2 + i;
                float width = (n - 1) * 0.15f;
                float centerX = cursorX + width * 0.5f;
                std::string nS = std::to_string(n);
                CreateSoftBodyCube(n, glm::vec3(centerX, 0.0f, 0.0f), "SBCube" + nS + "x" + nS + "x" + nS);

                cursorX += width + 0.1f;
            }
            //CreateSoftBodyCube(12, glm::vec3(0.0f, 0.0f, 0.0f), "SBCube12x12x12");

            first = false;
        }

        mAccumulatedTime += dt;
        float targetTime = 1.0f / 120.0f;
        auto& registry = ecs->GetRegistry();
        
        int count = 0;
        registry.view<TransformComponent, SoftBodyComponent>().each([&](auto entity, TransformComponent& tc, SoftBodyComponent& sb)
        {
            SyncSoftBodyWithTransform(sb, tc);
        });

        while (mAccumulatedTime > targetTime)
        {
            mAccumulatedTime -= targetTime;

            registry.view<TransformComponent, SoftBodyComponent>().each([&](auto entity, TransformComponent& tc, SoftBodyComponent& sb)
            {
                IntegrateSoftBodyRK4(sb, targetTime);
            });
        }

        registry.view<TransformComponent, SoftBodyComponent>().each([&](auto entity, TransformComponent& tc, SoftBodyComponent& sb)
        {
            DebugDrawSoftBodyFancy(sb);
        });
    }

    void PhysicsSystem::FrameEnd()
    {
    }

    void PhysicsSystem::SyncSoftBodyWithTransform(SoftBodyComponent& softBody, TransformComponent& transform)
    {
        glm::vec3 currentPos = transform.Translation;
        glm::vec3 delta = currentPos - softBody.lastTransformPosition;

        if (glm::length2(delta) < 1e-10f)
            return; // no meaningful movement

        for (auto& p : softBody.particles)
        {
            p.position += delta;
            p.anchorPosition += delta;
        }

        softBody.lastTransformPosition = currentPos;
    }

    void PhysicsSystem::CreateSoftBodyCube(int n, glm::vec3 position, std::string debugName)
    {
        // Create entity & add soft-body component.
        Entity entity = ecs->AddEntity(debugName);
        SoftBodyComponent& softBody = entity.AddComponent<SoftBodyComponent>();

        // Physics params.
        softBody.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        softBody.globalStiffness = 100.0f;
        softBody.globalDamping = 8.0f;
        softBody.lastTransformPosition = glm::vec3(0.0f);

        // Particles + springs.
        float totalMass = 12.5 * n * n;
        float massPerPoint = totalMass / static_cast<float>(n * n * n);

        InitializeSoftBodyParticles(softBody, n, n, n, 0.15f, massPerPoint);
        BuildSoftBodySprings(softBody);

        TransformComponent& transform = entity.GetComponent<TransformComponent>();
        transform.Translation = position;
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
            glm::vec3 pos = origin + spacing * glm::vec3(static_cast<float>(i), static_cast<float>(j), static_cast<float>(k));

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
        softBody.springs.reserve(300);

        auto& particles = softBody.particles;

        auto addSpring = [&](uint32_t a, uint32_t b)
        {
            SoftBodySpring spring{.a = a, .b = b};

            spring.restLength = glm::length(particles[b].position - particles[a].position);
            spring.stiffness = softBody.globalStiffness;
            spring.damping = softBody.globalDamping;

            softBody.springs.push_back(spring);
        };

        auto index = [&](uint32_t i, uint32_t j, uint32_t k) { return Index3D(i, j, k, nx, ny, nz); };

        for (uint32_t k = 0; k < nz; ++k)
        for (uint32_t j = 0; j < ny; ++j)
        for (uint32_t i = 0; i < nx; ++i)
        {
            uint32_t idx000 = index(i, j, k);

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
                uint32_t idx100 = index(i + 1, j, k);
                uint32_t idx010 = index(i, j + 1, k);
                uint32_t idx110 = index(i + 1, j + 1, k);

                addSpring(idx000, idx110); // (i,j,k)   <-> (i+1,j+1,k)
                addSpring(idx100, idx010); // (i+1,j,k) <-> (i,  j+1,k)
            }

            // XZ faces (constant j)
            if (i + 1 < nx && k + 1 < nz)
            {
                uint32_t idx100 = index(i + 1, j, k);
                uint32_t idx001 = index(i, j, k + 1);
                uint32_t idx101 = index(i + 1, j, k + 1);

                addSpring(idx000, idx101); // (i,j,k)   <-> (i+1,j,  k+1)
                addSpring(idx100, idx001); // (i+1,j,k) <-> (i,  j,  k+1)
            }

            // YZ faces (constant i)
            if (j + 1 < ny && k + 1 < nz)
            {
                uint32_t idx010 = index(i, j + 1, k);
                uint32_t idx001 = index(i, j, k + 1);
                uint32_t idx011 = index(i, j + 1, k + 1);

                addSpring(idx000, idx011); // (i,j,k)   <-> (i,  j+1,k+1)
                addSpring(idx010, idx001); // (i,j+1,k) <-> (i,  j,  k+1)
            }

            // -----------------------------
            // Body diagonals
            // -----------------------------
            if (i + 1 < nx && j + 1 < ny && k + 1 < nz)
            {
                uint32_t idx100 = index(i + 1, j, k);
                uint32_t idx010 = index(i, j + 1, k);
                uint32_t idx110 = index(i + 1, j + 1, k);
                uint32_t idx001 = index(i, j, k + 1);
                uint32_t idx101 = index(i + 1, j, k + 1);
                uint32_t idx011 = index(i, j + 1, k + 1);
                uint32_t idx111 = index(i + 1, j + 1, k + 1);

                // 4 space diagonals of the 2x2x2 cell
                addSpring(idx000, idx111); // 000 <-> 111
                addSpring(idx100, idx011); // 100 <-> 011
                addSpring(idx010, idx101); // 010 <-> 101
                addSpring(idx110, idx001); // 110 <-> 001
            }
        }
    }

    void PhysicsSystem::IntegrateSoftBodyRK4(SoftBodyComponent& softBody, float dt)
    {
        const std::size_t count = softBody.particles.size();
        if (count == 0) return;

        EnsureBufferCapacity(count);

        auto& x0 = mX0;
        auto& v0 = mV0;

        auto& k1x = mK1x;
        auto& k2x = mK2x;
        auto& k3x = mK3x;
        auto& k4x = mK4x;

        auto& k1v = mK1v;
        auto& k2v = mK2v;
        auto& k3v = mK3v;
        auto& k4v = mK4v;

        auto& xTmp = mXMid;
        auto& vTmp = mVMid;

        // ---------------------------------------------------------------------
        // Stage 0: initial state
        // ---------------------------------------------------------------------
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

        // ---------------------------------------------------------------------
        // k1: at (x0, v0)
        // ---------------------------------------------------------------------
        // acceleration -> k1v
        ComputeAccelerations(softBody, x0, v0, k1v);
        // velocity is derivative of position -> k1x
        for (std::size_t i = 0; i < count; ++i)
        {
            k1x[i] = v0[i];
        }

        // ---------------------------------------------------------------------
        // k2: at (x0 + 0.5*dt*k1x, v0 + 0.5*dt*k1v)
        // ---------------------------------------------------------------------
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                xTmp[i] = p.anchorPosition;
                vTmp[i] = glm::vec3(0.0f);
            }
            else
            {
                xTmp[i] = x0[i] + 0.5f * dt * k1x[i];
                vTmp[i] = v0[i] + 0.5f * dt * k1v[i];
            }
        }

        ComputeAccelerations(softBody, xTmp, vTmp, k2v);
        for (std::size_t i = 0; i < count; ++i)
        {
            k2x[i] = vTmp[i];
        }

        // ---------------------------------------------------------------------
        // k3: at (x0 + 0.5*dt*k2x, v0 + 0.5*dt*k2v)
        // ---------------------------------------------------------------------
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                xTmp[i] = p.anchorPosition;
                vTmp[i] = glm::vec3(0.0f);
            }
            else
            {
                xTmp[i] = x0[i] + 0.5f * dt * k2x[i];
                vTmp[i] = v0[i] + 0.5f * dt * k2v[i];
            }
        }

        ComputeAccelerations(softBody, xTmp, vTmp, k3v);
        for (std::size_t i = 0; i < count; ++i)
        {
            k3x[i] = vTmp[i];
        }

        // ---------------------------------------------------------------------
        // k4: at (x0 + dt*k3x, v0 + dt*k3v)
        // ---------------------------------------------------------------------
        for (std::size_t i = 0; i < count; ++i)
        {
            const SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                xTmp[i] = p.anchorPosition;
                vTmp[i] = glm::vec3(0.0f);
            }
            else
            {
                xTmp[i] = x0[i] + dt * k3x[i];
                vTmp[i] = v0[i] + dt * k3v[i];
            }
        }

        ComputeAccelerations(softBody, xTmp, vTmp, k4v);
        for (std::size_t i = 0; i < count; ++i)
        {
            k4x[i] = vTmp[i];
        }

        // ---------------------------------------------------------------------
        // Final update
        // ---------------------------------------------------------------------
        float inv6dt = dt / 6.0f;

        for (std::size_t i = 0; i < count; ++i)
        {
            SoftBodyParticle& p = softBody.particles[i];

            if (p.isAnchor || p.inverseMass == 0.0f)
            {
                p.position = p.anchorPosition;
                p.velocity = glm::vec3(0.0f);
                continue;
            }

            glm::vec3 dx = (k1x[i] + 2.0f * k2x[i] + 2.0f * k3x[i] + k4x[i]) * inv6dt;
            glm::vec3 dv = (k1v[i] + 2.0f * k2v[i] + 2.0f * k3v[i] + k4v[i]) * inv6dt;

            p.position = x0[i] + dx;
            p.velocity = v0[i] + dv;
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
            uint32_t ia = spring.a;
            uint32_t ib = spring.b;

            const glm::vec3& xa = positions[ia];
            const glm::vec3& xb = positions[ib];
            const glm::vec3& va = velocities[ia];
            const glm::vec3& vb = velocities[ib];

            glm::vec3 delta = xb - xa;
            float length = glm::length(delta);
            if (length < 1e-6f) continue;

            glm::vec3 dir = delta / length;

            float stretch = length - spring.restLength;
            float relativeSpeed = glm::dot(vb - va, dir);

            // Force on particle b
            glm::vec3 force = -spring.stiffness * stretch * dir
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
        mK1x.resize(count);
        mK2x.resize(count);
        mK3x.resize(count);
        mK4x.resize(count);
        mK1v.resize(count);
        mK2v.resize(count);
        mK3v.resize(count);
        mK4v.resize(count);
    }

    void PhysicsSystem::DebugDrawSoftBodyFancy(const SoftBodyComponent& softBody)
    {
        const auto& particles = softBody.particles;
        const auto& springs = softBody.springs;

        if (particles.empty())
            return;

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

        // Pre-pass: compute max speed for color normalization
        float maxSpeed = 0.0f;
        for (const SoftBodyParticle& p : particles)
        {
            if (p.isAnchor) continue;
            maxSpeed = std::max(maxSpeed, glm::length(p.velocity));
        }
        if (maxSpeed < 1e-4f) maxSpeed = 1e-4f;


        // Springs Rendering
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
            if (strain >= 0.0f) springColor = glm::mix(springNeutralColor, springStretchedColor, t);
            else springColor = glm::mix(springNeutralColor, springCompressedColor, t);

            float thickness = baseSpringThickness * (1.0f + 1.5f * t);

            DebugDrawResource::DrawLine(pa, pb, springColor, thickness);
        }

        // Particles Rendering
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
