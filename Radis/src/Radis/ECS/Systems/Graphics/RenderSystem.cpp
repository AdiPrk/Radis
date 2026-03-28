/*****************************************************************//**
 * \file   RenderSystem.cpp
 * \brief  Handles rendering the scene!
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "RenderSystem.h"

#include "ECS/Resources/renderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/DebugDrawResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/AnimationResource.h"
#include "ECS/Resources/SwapRendererResource.h"
#include "ECS/Resources/RaytracingResource.h"

#include "../InputSystem.h"

#include "Graphics/Vulkan/Uniform/UniformData.h"
#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"
#include "Graphics/Vulkan/Pipeline/ComputePipeline.h"
#include "Graphics/Vulkan/Pipeline/RaytracingPipeline.h"
#include "Graphics/Common/Model.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Common/Animation/AnimationLibrary.h"
#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Vulkan/Utils/ScopedDebugLabel.h"
#include "Graphics/Common/UnifiedMesh.h"

#include "ECS/ECS.h"
#include "ECS/Entities/Entity.h"
#include "ECS/Components/Components.h"

#include "Engine.h"
#include "Graphics/OpenGL/GLFrameBuffer.h"
#include "Graphics/OpenGL/GLTexture.h"
#include "Graphics/IWindow.h"

namespace Radis
{
    RenderSystem::RenderSystem() : ISystem("RenderSystem") {}
    RenderSystem::~RenderSystem() {}

    void RenderSystem::Init()
    {
        // Pre-allocate reasonable initial capacity to avoid early reallocations
        mInstanceData.reserve(1024);
        mDrawCalls.reserve(128);
        mLightData.reserve(64);
        mMeshInstanceCounts.reserve(128);
    }

    void RenderSystem::Exit()
    {
    }

    void RenderSystem::FrameStart()
    {
        auto rr = ecs->GetResource<RenderingResource>();

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan && !rr->tlasAccel.accel && rr->blasAccel.empty())
        {
            // get num entities with model component
            mRTMeshData.clear();
            mRTMeshIndices.clear();
            
            auto uMeshes = rr->modelLibrary->GetUnifiedMesh();
            if (uMeshes)
            {            
                MeshDataUniform vertexData;
                for (auto& v : uMeshes->GetUnifiedMesh().mVertices)
                {
                    vertexData.posX = v.position.x;
                    vertexData.posY = v.position.y;
                    vertexData.posZ = v.position.z;
                    vertexData.colorR = v.color.r;
                    vertexData.colorG = v.color.g;
                    vertexData.colorB = v.color.b;
                    vertexData.normalX = v.normal.x;
                    vertexData.normalY = v.normal.y;
                    vertexData.normalZ = v.normal.z;
                    vertexData.texU = v.uv.x;
                    vertexData.texV = v.uv.y;

                    mRTMeshData.push_back(vertexData);
                }
            
                mRTMeshIndices = uMeshes->GetUnifiedMesh().mIndices;
            }
            
            auto rtr = ecs->GetResource<RaytracingResource>();
            rtr->CreateBLAS();  // Set up BLAS infrastructure
            rtr->CreateTLAS();  // Set up TLAS infrastructure
            
            VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
            asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            asInfo.accelerationStructureCount = 1;
            asInfo.pAccelerationStructures = &rr->tlasAccel.accel;
            for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
            {
                DescriptorWriter writer(*rr->rtUniform->GetDescriptorLayout(), *rr->rtUniform->GetDescriptorPool());
                writer.WriteAccelerationStructure(0, &asInfo);
                writer.Overwrite(rr->rtUniform->GetDescriptorSets()[frameIndex]);

                rr->rtUniform->SetUniformData(mRTMeshData, 3, frameIndex);
                rr->rtUniform->SetUniformData(mRTMeshIndices, 4, frameIndex);
            }
        }

        // Update textures!
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            rr->modelLibrary->QueueTextures();
        }

        rr->textureLibrary->LoadQueuedTextures();
        rr->textureLibrary->UpdateTextureUniform(rr->cameraUniform.get());

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            rr->textureLibrary->UpdateRTUniform(*rr);
        }

        // Draw the editor grid
        // DebugDrawResource::DrawEditorGrid(50, 1.0f);

        // Swap renderer backends
        // ecs->GetResource<SwapRendererResource>()->RequestSwap();
    }

    void RenderSystem::Update(float dt)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        float aspectRatio = GetAspectRatio();
        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();
        UnifiedMeshes* uMeshes = ml->GetUnifiedMesh();

        // Collect Uniform Data
        CameraUniforms camData = CollectCameraData(aspectRatio);
        CollectLightData();

        // Build Instances
        BuildInstanceData();

        mShadowCamData = {};
        bool foundShadowCam = false;
        ecs->GetRegistry().view<LightComponent, TransformComponent>().each([&](auto, LightComponent& lc, TransformComponent& tc)
        {
            if (foundShadowCam) return;
            if (lc.LightType != LightComponent::Types::Directional) return;

            glm::vec3 L = glm::normalize(lc.Direction);
            glm::vec3 up = (std::abs(L.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

            // Fixed stable shadow frustum
            constexpr float kShadowHalfWidth = 15.0f; // tune to scene size
            constexpr float kShadowNear = 0.1f;
            constexpr float kShadowFar = 35.0f;
            constexpr float kDistBack = 15.0f;

            glm::vec3 sceneCenter(0.0f);
            glm::vec3 eye = sceneCenter - L * kDistBack;
            glm::mat4 lightView = glm::lookAt(eye, sceneCenter, up);
            glm::mat4 lightProj = glm::orthoRH_ZO(
                -kShadowHalfWidth, kShadowHalfWidth,
                -kShadowHalfWidth, kShadowHalfWidth,
                kShadowNear, kShadowFar
            );
            lightProj[1][1] *= -1;

            mShadowCamData.lightView = lightView;
            mShadowCamData.lightViewProj = lightProj * lightView;
            mShadowCamData.z0 = kShadowNear;
            mShadowCamData.z1 = kShadowFar;

            foundShadowCam = true;
        });

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto ar = ecs->GetResource<AnimationResource>();

            rr->cameraUniform->SetUniformData(camData, 0, rr->currentFrameIndex);                  // Set Camera Data
            rr->cameraUniform->SetUniformData(mInstanceData, 1, rr->currentFrameIndex);            // Set Instance Data
            rr->cameraUniform->SetUniformData(ar->bonesMatrices, 2, rr->currentFrameIndex);        // Set Animation Data
            rr->cameraUniform->SetUniformData(mLightBuffer, 4, rr->currentFrameIndex);             // Set Light Data
            rr->shadowMomentsUniform->SetUniformData(mShadowCamData, 0, rr->currentFrameIndex);     // Set Shadow Camera Data
            rr->shadowMomentsUniform->SetUniformData(mInstanceData, 1, rr->currentFrameIndex);     // Set Instance Data
            rr->shadowMomentsUniform->SetUniformData(ar->bonesMatrices, 2, rr->currentFrameIndex); // Set Animation Data

            rr->deferredLightingUniform->SetUniformData(camData, 0, rr->currentFrameIndex);      // Camera data
            rr->deferredLightingUniform->SetUniformData(mLightBuffer, 6, rr->currentFrameIndex); // Light data
            UpdateShadowParamsUBO(*rr, rr->currentFrameIndex);

            // Add Render Passes!
            auto& rg = rr->renderGraph;
            std::string colorWriteTarget = Engine::GetEditorEnabled() ? "SceneTexture" : "BackBuffer";

            switch (rr->renderMode)
            {
            case RenderMode::Forward: {
                rg->AddPass(
                    "ScenePass",
                    [&](RGPassBuilder& b)
                    {
                        b.writes(colorWriteTarget);
                        b.writes("SceneDepth");
                    },
                    std::bind(&RenderSystem::RenderSceneVK, this, std::placeholders::_1)
                );
                break;
            }
/*            case RenderMode::Deferred: {
                // MSM pass
                rg->AddPass("ShadowMomentsPass",
                    [&](RGPassBuilder& b) {
                        b.writes("ShadowMomentsRaw");
                        b.writes("ShadowDepth");
                    },
                    std::bind(&RenderSystem::RenderShadowMomentsVK, this, std::placeholders::_1)
                );

                rg->AddPass("ShadowBlurH",
                    [&](RGPassBuilder& b) {
                        b.setCompute();
                        b.reads("ShadowMomentsRaw");
                        b.writes("ShadowMomentsTmp");
                    },
                    std::bind(&RenderSystem::RenderShadowBlurHVK, this, std::placeholders::_1)
                );
                
                rg->AddPass("ShadowBlurV",
                    [&](RGPassBuilder& b) {
                        b.setCompute(); 
                        b.reads("ShadowMomentsTmp");
                        b.writes("ShadowMoments");
                    },
                    std::bind(&RenderSystem::RenderShadowBlurVVK, this, std::placeholders::_1)
                );

                // G-Buffer pass
                rg->AddPass("GBufferPass",
                    [&](RGPassBuilder& b) {
                        b.writes("gAlbedo");
                        b.writes("gNormal");
                        b.writes("gPBR");
                        b.writes("gEmissive");
                        b.writes("SceneDepth");
                    },
                    std::bind(&RenderSystem::RenderSceneDeferredGeometryVK, this, std::placeholders::_1)
                );

                // Lighting pass - directional/ambient -> raw HDR to SceneHDR
                rg->AddPass("LightingPass",
                    [&](RGPassBuilder& b) {
                        b.reads("gAlbedo");
                        b.reads("gNormal");
                        b.reads("gPBR");
                        b.reads("gEmissive");
                        b.reads("SceneDepth");
                        b.reads("ShadowMoments");
                        b.writes("SceneHDR");
                    },
                    std::bind(&RenderSystem::RenderSceneDeferredLightingVK, this, std::placeholders::_1)
                );

                // Light volumes pass - additive local lights -> SceneHDR
                rg->AddPass("LightVolumesPass",
                    [&](RGPassBuilder& b) {
                        b.reads("gAlbedo");
                        b.reads("gNormal");
                        b.reads("gPBR");
                        b.reads("gEmissive");
                        b.reads("SceneDepth");
                        b.writes("SceneHDR");
                    },
                    std::bind(&RenderSystem::RenderLightVolumesVK, this, std::placeholders::_1)
                );

                // Tone map pass - reads accumulated HDR, writes final output
                rg->AddPass("ToneMapPass",
                    [&](RGPassBuilder& b) {
                        b.reads("SceneHDR");
                        b.writes(colorWriteTarget);
                    },
                    std::bind(&RenderSystem::RenderToneMapVK, this, std::placeholders::_1)
                );

                break;
            }*/
            case RenderMode::Deferred: {
                // G-Buffer pass
                rg->AddPass("GBufferPass",
                    [&](RGPassBuilder& b) {
                        b.writes("gAlbedo");
                        b.writes("gNormal");
                        b.writes("gPBR");
                        b.writes("gEmissive");
                        b.writes("SceneDepth");
                    },
                    std::bind(&RenderSystem::RenderSceneDeferredGeometryVK, this, std::placeholders::_1)
                );

                // Lighting pass - directional/ambient -> raw HDR to SceneHDR
                rg->AddPass("LightingPass",
                    [&](RGPassBuilder& b) {
                        b.reads("gAlbedo");
                        b.reads("gNormal");
                        b.reads("gPBR");
                        b.reads("gEmissive");
                        b.reads("SceneDepth");
                        b.reads("ShadowMoments");
                        b.writes("SceneHDR");
                    },
                    std::bind(&RenderSystem::RenderSceneDeferredLightingVK, this, std::placeholders::_1)
                );

                // Light volumes pass - additive local lights -> SceneHDR
                rg->AddPass("LightVolumesPass",
                    [&](RGPassBuilder& b) {
                        b.reads("gAlbedo");
                        b.reads("gNormal");
                        b.reads("gPBR");
                        b.reads("gEmissive");
                        b.reads("SceneDepth");
                        b.writes("SceneHDR");
                    },
                    std::bind(&RenderSystem::RenderLightVolumesVK, this, std::placeholders::_1)
                );

                // Tone map pass - reads accumulated HDR, writes final output
                rg->AddPass("ToneMapPass",
                    [&](RGPassBuilder& b) {
                        b.reads("SceneHDR");
                        b.writes(colorWriteTarget);
                    },
                    std::bind(&RenderSystem::RenderToneMapVK, this, std::placeholders::_1)
                );

                break;
            }
            case RenderMode::Raytracing: {
                rg->AddPass(
                    "ScenePass",
                    [&](RGPassBuilder& b) {},
                    std::bind(&RenderSystem::RaytraceSceneVK, this, std::placeholders::_1)
                );
                break;
            }
            }
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            auto ar = ecs->GetResource<AnimationResource>();
            rr->shader->Use();
            rr->shader->SetCameraUBO(camData);

            GLShader::SetupAnimationSSBO();
            GLuint animationVBO = GLShader::GetAnimationSSBO();
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, animationVBO);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, ar->bonesMatrices.size() * sizeof(VQS), ar->bonesMatrices.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            RenderSceneGL();
        }
    }

    void RenderSystem::FrameEnd()
    {
    }

    void RenderSystem::ExecuteInstancedDrawCalls(VkCommandBuffer cmd)
    {
        for (const auto& drawCall : mDrawCalls)
        {
            vkCmdDrawIndexed(
                cmd,
                drawCall.indexCount,
                drawCall.instanceCount,
                drawCall.firstIndex,
                drawCall.vertexOffset,
                drawCall.firstInstance
            );
        }
    }

    void RenderSystem::RenderSceneVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        UnifiedMeshes* uMeshes = rr->modelLibrary->GetUnifiedMesh();
        ScopedDebugLabel sceneDebugLabel(rr->device.get(), cmd, "Render Scene", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

        // Bind Pipeline, Uniforms, and Mesh
        auto& pipeline = rr->renderWireframe ? rr->wireframePipeline : rr->pipeline;
        pipeline->Bind(cmd);
        rr->cameraUniform->Bind(cmd, pipeline->GetLayout(), rr->currentFrameIndex);
        SetViewportAndScissor(cmd, rr->swapChain->GetSwapChainExtent());
        uMeshes->GetUnifiedMesh().Bind(cmd);

        // Execute Draw Calls
        ExecuteInstancedDrawCalls(cmd);
    }

    void RenderSystem::RenderShadowMomentsVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        ScopedDebugLabel label(rr->device.get(), cmd, "Shadow Moments Pass", glm::vec4(0.2f, 0.2f, 0.9f, 1.0f));

        // Bind pipeline
        rr->shadowMomentsPipeline->Bind(cmd);

        // Bind shadow uniform (light matrices + instances)
        rr->shadowMomentsUniform->Bind(cmd, rr->shadowMomentsPipeline->GetLayout(), rr->currentFrameIndex);

        // Set viewport/scissor to shadow resolution
        VKTexture* sm = static_cast<VKTexture*>(rr->textureLibrary->GetTexture("ShadowMomentsRaw"));
        VkExtent2D ext{ (uint32_t)sm->GetWidth(), (uint32_t)sm->GetHeight() };
        SetViewportAndScissor(cmd, ext);

        // Bind unified mesh + draw instances
        UnifiedMeshes* uMeshes = rr->modelLibrary->GetUnifiedMesh();
        uMeshes->GetUnifiedMesh().Bind(cmd);
        ExecuteInstancedDrawCalls(cmd);
    }

    static inline uint32_t DivUp(uint32_t a, uint32_t b) { return (a + b - 1) / b; }

    void RenderSystem::RenderShadowBlurHVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        ScopedDebugLabel label(rr->device.get(), cmd, "Shadow Blur H", glm::vec4(0.2f, 0.6f, 0.9f, 1.0f));

        rr->shadowBlurHPipeline->Bind(cmd);
        rr->shadowBlurHUniform->Bind(cmd, rr->shadowBlurHPipeline->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_COMPUTE);

        VKTexture* src = static_cast<VKTexture*>(rr->textureLibrary->GetTexture("ShadowMomentsRaw"));

        rr->msmPC.width = (int)src->GetWidth();
        rr->msmPC.height = (int)src->GetHeight();

        vkCmdPushConstants(cmd, rr->shadowBlurHPipeline->GetLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MSMBlurPC), &rr->msmPC);

        const uint32_t gx = DivUp(src->GetWidth(), 16);
        const uint32_t gy = DivUp(src->GetHeight(), 16);
        vkCmdDispatch(cmd, gx, gy, 1);
    }

    void RenderSystem::RenderShadowBlurVVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        ScopedDebugLabel label(rr->device.get(), cmd, "Shadow Blur V", glm::vec4(0.2f, 0.6f, 0.9f, 1.0f));

        rr->shadowBlurVPipeline->Bind(cmd);
        rr->shadowBlurVUniform->Bind(cmd, rr->shadowBlurVPipeline->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_COMPUTE);

        VKTexture* src = static_cast<VKTexture*>(rr->textureLibrary->GetTexture("ShadowMomentsTmp"));

        rr->msmPC.width = (int)src->GetWidth();
        rr->msmPC.height = (int)src->GetHeight();

        vkCmdPushConstants(cmd, rr->shadowBlurVPipeline->GetLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MSMBlurPC), &rr->msmPC);

        const uint32_t gx = DivUp(src->GetWidth(), 16);
        const uint32_t gy = DivUp(src->GetHeight(), 16);
        vkCmdDispatch(cmd, gx, gy, 1);
    }

    void RenderSystem::RenderSceneGL()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto er = ecs->GetResource<EditorResource>();

        rr->shader->Use();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (Engine::GetEditorEnabled()) {
            rr->sceneFrameBuffer->Bind();
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        std::vector<uint64_t> textureData{};
        for (uint32_t i = 0; i < rr->textureLibrary->GetTextureCount(); ++i)
        {
            if (auto itex = rr->textureLibrary->GetTextureByIndex(i))
            {
                GLTexture* gltex = static_cast<GLTexture*>(itex);
                textureData.push_back(gltex->textureHandle);
            }
        }

        GLShader::SetupInstanceSSBO();
        GLuint iSSBO = GLShader::GetInstanceSSBO();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, iSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, mInstanceData.size() * sizeof(InstanceUniforms), mInstanceData.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        GLShader::SetupTextureSSBO();
        GLuint textureSSBO = GLShader::GetTextureSSBO();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, textureData.size() * sizeof(uint64_t), textureData.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        GLShader::SetupLightSSBO();
        GLuint lightSSBO = GLShader::GetLightSSBO();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, mLightBuffer.size(), mLightBuffer.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        UnifiedMeshes* uMeshes = rr->modelLibrary->GetUnifiedMesh();
        uMeshes->GetUnifiedMesh().Bind();

        for (const auto& drawCall : mDrawCalls)
        {
            glDrawElementsInstancedBaseVertexBaseInstance(
                GL_TRIANGLES,
                static_cast<GLsizei>(drawCall.indexCount),
                GL_UNSIGNED_INT,
                (void*)(sizeof(uint32_t) * drawCall.firstIndex),
                drawCall.instanceCount,
                drawCall.vertexOffset,
                drawCall.firstInstance
            );
        }

        if (Engine::GetEditorEnabled())
        {
            rr->sceneFrameBuffer->Unbind();
        }
    }

    float RenderSystem::GetAspectRatio()
    {
        if (Engine::GetEditorEnabled())
        {
            EditorResource* er = ecs->GetResource<EditorResource>();
            return er->sceneWindowWidth / er->sceneWindowHeight;
        }
        
        WindowResource* wr = ecs->GetResource<WindowResource>();
        glm::uvec2 extant = wr->window->GetExtent();
        return static_cast<float>(extant.x) / static_cast<float>(extant.y);
    }

    // Utilities
    void RenderSystem::UpdateShadowParamsUBO(RenderingResource& rr, int frameIndex)
    {
        ShadowParamsUniform sp{};

        sp.lightViewProj = mShadowCamData.lightViewProj;
        sp.lightView = mShadowCamData.lightView;
        float z0 = mShadowCamData.z0;
        float z1 = mShadowCamData.z1;

        float invRange = 1.0f / std::max(z1 - z0, 1e-6f);

        const float alpha = 1e-5f;
        sp.zParams = glm::vec4(z0, z1, invRange, alpha);

        VKTexture* sm = static_cast<VKTexture*>(rr.textureLibrary->GetTexture("ShadowMoments"));
        sp.mapParams = glm::vec4(1.0f / sm->GetWidth(), 1.0f / sm->GetHeight(), 7.5f, 0.0f);

        // Write to deferred lighting UBO binding
        rr.deferredLightingUniform->SetUniformData(sp, 8, frameIndex);
    }

    void RenderSystem::SetViewportAndScissor(VkCommandBuffer cmd, const VkExtent2D& extent)
    {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, extent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    CameraUniforms RenderSystem::CollectCameraData(float aspectRatio)
    {
        CameraUniforms camData{};
        camData.view = glm::mat4(1.0f);
        camData.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);

        Entity cameraEntity = ecs->GetEntity("Camera");
        if (cameraEntity)
        {
            TransformComponent& tc = cameraEntity.GetComponent<TransformComponent>();
            CameraComponent& cc = cameraEntity.GetComponent<CameraComponent>();

            glm::vec3 cameraPos = tc.Translation;
            glm::vec3 cameraTarget = cameraPos + glm::normalize(cc.Forward);

            camData.view = glm::lookAt(cameraPos, cameraTarget, glm::normalize(cc.Up));
            camData.cameraPos = glm::vec4(tc.Translation, 1.0f);
            camData.projection = glm::perspective(glm::radians(cc.FOV), aspectRatio, cc.Near, cc.Far);
        }

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
            camData.projection[1][1] *= -1;

        camData.projectionView = camData.projection * camData.view;
        camData.inverseProjView = glm::inverse(camData.projectionView);
        return camData;
    }

    void RenderSystem::CollectLightData()
    {
        mLightData.clear();
        mDirectionalLightCount = 0;
        mLocalLightCount = 0;
        auto& registry = ecs->GetRegistry();

        // Pass 1: Directional lights first
        registry.view<LightComponent, TransformComponent>().each(
            [this](auto, LightComponent& lc, TransformComponent& tc)
            {
                if (lc.LightType != LightComponent::Types::Directional) return; // Skip non-directional
                mLightData.push_back({
                    .positionRadius = glm::vec4(tc.Translation, lc.Radius),
                    .colorIntensity = glm::vec4(lc.Color, lc.Intensity),
                    .directionInner = glm::vec4(glm::normalize(lc.Direction), lc.InnerCone),
                    .outerConeType = glm::vec4(lc.OuterCone, static_cast<float>(lc.LightType), 0.0f, 0.0f)
                    });
            });
        mDirectionalLightCount = static_cast<uint32_t>(mLightData.size());

        // Pass 2: Local lights (point + spot) after directional
        registry.view<LightComponent, TransformComponent>().each(
            [this](auto, LightComponent& lc, TransformComponent& tc)
            {
                if (lc.LightType == LightComponent::Types::Directional) return; // Skip directional
                mLightData.push_back({
                    .positionRadius = glm::vec4(tc.Translation, lc.Radius),
                    .colorIntensity = glm::vec4(lc.Color, lc.Intensity),
                    .directionInner = glm::vec4(glm::normalize(lc.Direction), lc.InnerCone),
                    .outerConeType = glm::vec4(lc.OuterCone, static_cast<float>(lc.LightType), 0.0f, 0.0f)
                    });
            });
        mLocalLightCount = static_cast<uint32_t>(mLightData.size()) - mDirectionalLightCount;

        struct LightHeader { uint32_t lightCount; uint32_t _pad[3]; };
        LightHeader header{ .lightCount = static_cast<uint32_t>(mLightData.size()) };
        mLightBuffer.resize(sizeof(LightHeader) + sizeof(LightUniform) * mLightData.size());
        memcpy(mLightBuffer.data(), &header, sizeof(LightHeader));
        memcpy(mLightBuffer.data() + sizeof(LightHeader), mLightData.data(),
            sizeof(LightUniform) * mLightData.size());
    }

    void RenderSystem::BuildInstanceData()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto& registry = ecs->GetRegistry();
        ModelLibrary* ml = rr->modelLibrary.get();
        AnimationLibrary* al = rr->animationLibrary.get();
        UnifiedMeshes* uMeshes = ml->GetUnifiedMesh();

        // ============================================================
        // Two-pass instancing: Count first, then fill
        for (auto& [meshID, count] : mMeshInstanceCounts)
        {
            count = 0;
        }

        // Debug draw instances (all use cube mesh)
        const auto& debugData = DebugDrawResource::GetInstanceData();
        uint32_t debugDrawCount = 0;
        uint32_t cubeMeshID = 0;
        Model* cubeModel = nullptr;

        if (rr->renderMode != RenderMode::Raytracing && !debugData.empty())
        {
            cubeModel = ml->TryAddGetModel("Assets/Models/cube.obj");
            if (cubeModel && !cubeModel->mMeshes.empty())
            {
                cubeMeshID = cubeModel->mMeshes[0]->GetID();
                debugDrawCount = static_cast<uint32_t>(debugData.size());
                mMeshInstanceCounts[cubeMeshID] += debugDrawCount;
            }
        }

        // Count instances per mesh
        auto modelTransformEntities = registry.view<ModelComponent, TransformComponent>();
        modelTransformEntities.each([&](auto entity, ModelComponent& mc, TransformComponent& tc)
        {
            Model* model = ml->TryAddGetModel(mc);
            if (!model) return;

            for (auto& mesh : model->mMeshes)
            {
                mMeshInstanceCounts[mesh->GetID()]++;
            }
        });

        // Calculate total instances and build draw calls with offsets
        uint32_t totalInstances = 0;
        mDrawCalls.clear();

        for (auto& [meshID, count] : mMeshInstanceCounts)
        {
            if (count == 0) continue;

            const MeshInfo& meshInfo = uMeshes->GetMeshInfo(meshID);

            InstancedDrawCall& drawCall = mDrawCalls.emplace_back();
            drawCall.meshID = meshID;
            drawCall.indexCount = meshInfo.indexCount;
            drawCall.firstIndex = meshInfo.firstIndex;
            drawCall.vertexOffset = meshInfo.vertexOffset;
            drawCall.instanceCount = count;
            drawCall.firstInstance = totalInstances;

            totalInstances += count;
        }

        // Resize instance buffer (only reallocates if capacity exceeded)
        mInstanceData.resize(totalInstances);

        // Build a map from meshID -> current write index
        std::unordered_map<uint32_t, uint32_t> meshWriteIndex;
        meshWriteIndex.reserve(mDrawCalls.size());
        for (const auto& drawCall : mDrawCalls)
        {
            meshWriteIndex[drawCall.meshID] = drawCall.firstInstance;
        }

        // Pass 2: Fill debug draw instances
        if (debugDrawCount > 0 && cubeModel)
        {
            uint32_t writeIdx = meshWriteIndex[cubeMeshID];
            std::memcpy(&mInstanceData[writeIdx], debugData.data(), debugDrawCount * sizeof(InstanceUniforms));
            meshWriteIndex[cubeMeshID] += debugDrawCount;
        }

        // Pass 2: Fill entity instances directly into final positions
        modelTransformEntities.each([&](auto entity, ModelComponent& mc, TransformComponent& tc)
        {
            Model* model = ml->GetModel(mc);
            if (!model) return;

            AnimationComponent* ac = registry.try_get<AnimationComponent>(entity);

            uint32_t boneOffset = AnimationLibrary::INVALID_ANIMATION_INDEX;
            if (ac && al->GetAnimation(ac->AnimationIndex) && al->GetAnimator(ac->AnimationIndex))
            {
                boneOffset = ac->BoneOffset;
            }

            for (auto& mesh : model->mMeshes)
            {
                uint32_t meshID = mesh->GetID();
                uint32_t writeIdx = meshWriteIndex[meshID]++;

                InstanceUniforms& data = mInstanceData[writeIdx];

                if (boneOffset == AnimationLibrary::INVALID_ANIMATION_INDEX && mc.NormalizeModel)
                {
                     data.model = tc.GetTransform() * model->GetNormalizationMatrix();
                }
                else
                {
                    data.model = tc.GetTransform();
                }
                
                const MeshInfo& meshInfo = uMeshes->GetMeshInfo(meshID);
                float meshMetallic = mc.UseMetallicOverride ? mc.MetallicOverride : mesh->metallicFactor;
                float meshRoughness = mc.UseRoughnessOverride ? mc.RoughnessOverride : mesh->roughnessFactor;
                glm::vec4 meshEmissive = mc.UseEmissiveOverride ? mc.EmissiveOverride : mesh->emissiveFactor;

                uint32_t metallicIndex = mc.UseMetallicOverride ? TextureLibrary::INVALID_TEXTURE_INDEX : mesh->metalnessTextureIndex;
                uint32_t roughnessIndex = mc.UseRoughnessOverride ? TextureLibrary::INVALID_TEXTURE_INDEX : mesh->roughnessTextureIndex;
                if (mesh->mMetallicRoughnessCombined) roughnessIndex = metallicIndex;

                data.tint = mc.TintColor;
                data.textureIndices = glm::uvec4(mesh->albedoTextureIndex, mesh->normalTextureIndex, metallicIndex, roughnessIndex);
                data.textureIndices2 = glm::uvec4(mesh->occlusionTextureIndex, mesh->emissiveTextureIndex, 10001, 10001);
                data.boneOffset = boneOffset;
                data.baseColorFactor = mesh->baseColorFactor;
                data.metallicRoughnessFactor = glm::vec4(meshMetallic, meshRoughness, 0.f, 0.f);
                data.emissiveFactor = meshEmissive;
                data.indexOffset = meshInfo.firstIndex;
                data.vertexOffset = meshInfo.vertexOffset;
                data.meshID = meshID;
            }
        });
    }

    void RenderSystem::RaytraceSceneVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto& rp = rr->raytracingPipeline;
        ScopedDebugLabel rtDebugLabel(rr->device.get(), cmd, "Raytrace Scene", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

        // Update TLAS with mInstanceData
        {
            std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
            tlasInstances.reserve(mInstanceData.size());
            for (size_t i = 0; i < mInstanceData.size(); ++i)
            {
                const auto& instanceData = mInstanceData[i];
                VkAccelerationStructureInstanceKHR asInstance{};
                asInstance.transform = ToTransformMatrixKHR(instanceData.model);
                asInstance.instanceCustomIndex = static_cast<uint32_t>(i);
                asInstance.accelerationStructureReference = rr->blasAccel[instanceData.meshID].address;
                asInstance.instanceShaderBindingTableRecordOffset = 0;
                asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                asInstance.mask = 0xFF;
                tlasInstances.emplace_back(asInstance);
            }

            if (tlasInstances.empty())
            {
                VkAccelerationStructureInstanceKHR asInstance{};
                asInstance.transform = ToTransformMatrixKHR(glm::mat4(0.0f));
                asInstance.instanceCustomIndex = 0;
                asInstance.accelerationStructureReference = rr->blasAccel[0].address;
                asInstance.instanceShaderBindingTableRecordOffset = 0;
                asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                asInstance.mask = 0xFF;
                tlasInstances.emplace_back(asInstance);
            }

            auto rtr = ecs->GetResource<RaytracingResource>();
            rtr->UpdateTopLevelASImmediate(tlasInstances);
        }

        // Bind Pipeline and Uniforms
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rp->GetPipeline());
        rr->cameraUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
        rr->rtUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

        // Trace Rays!
        const VkExtent2D& size = rr->swapChain->GetSwapChainExtent();
        vkCmdTraceRaysKHR(cmd, &rp->GetRaygenRegion(), &rp->GetMissRegion(), &rp->GetHitRegion(), &rp->GetCallableRegion(), size.width, size.height, 1);

        // Synchronize ray tracing writes with subsequent reads
        VkMemoryBarrier2 memoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
        };

        VkDependencyInfo dependencyInfo = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &memoryBarrier
        };

        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // -----------------------------------------
    // Deferred Rendering Passes
    // -----------------------------------------

    void RenderSystem::RenderSceneDeferredGeometryVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        UnifiedMeshes* uMeshes = rr->modelLibrary->GetUnifiedMesh();
        ScopedDebugLabel sceneDebugLabel(rr->device.get(), cmd, "Deferred G-Buffer Pass", glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));

        // Bind Pipeline, Uniforms, and Mesh
        auto& pipeline = rr->gBufferPipeline;
        pipeline->Bind(cmd);
        rr->cameraUniform->Bind(cmd, pipeline->GetLayout(), rr->currentFrameIndex);
        SetViewportAndScissor(cmd, rr->swapChain->GetSwapChainExtent());
        uMeshes->GetUnifiedMesh().Bind(cmd);

        // Execute Draw Calls
        ExecuteInstancedDrawCalls(cmd);
    }

    void RenderSystem::RenderSceneDeferredLightingVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        ScopedDebugLabel sceneDebugLabel(rr->device.get(), cmd, "Deferred Lighting Pass", glm::vec4(0.8f, 0.8f, 0.2f, 1.0f));

        // Bind Pipeline and Uniforms
        auto& pipeline = rr->deferredLightingPipeline;
        pipeline->Bind(cmd);
        rr->deferredLightingUniform->Bind(cmd, pipeline->GetLayout(), rr->currentFrameIndex);
        SetViewportAndScissor(cmd, rr->swapChain->GetSwapChainExtent());

        // Draw Fullscreen Quad
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    void RenderSystem::RenderLightVolumesVK(VkCommandBuffer cmd)
    {
        if (mLocalLightCount == 0) return;

        auto rr = ecs->GetResource<RenderingResource>();
        ModelLibrary* ml = rr->modelLibrary.get();
        UnifiedMeshes* uMeshes = ml->GetUnifiedMesh();
        ScopedDebugLabel label(rr->device.get(), cmd, "Light Volumes Pass", glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));

        // Load sphere mesh on first use
        Model* sphereModel = ml->TryAddGetModel("Assets/Models/sphere.glb");
        if (!sphereModel) return;

        uint32_t sphereID = sphereModel->mMeshes[0]->GetID();
        
        // Bind pipeline, uniforms, viewport, mesh
        rr->lightVolumePipeline->Bind(cmd);
        rr->deferredLightingUniform->Bind(cmd, rr->lightVolumePipeline->GetLayout(), rr->currentFrameIndex);
        SetViewportAndScissor(cmd, rr->swapChain->GetSwapChainExtent());
        uMeshes->GetUnifiedMesh().Bind(cmd);

        // Push constants: light offset + debug mode
        LightVolumePushConstants pc{};
        pc.directionalLightCount = mDirectionalLightCount;
        pc.debugMode = rr->lightVolumeDebugMode;

        auto tex = rr->textureLibrary->GetTexture("gAlbedo");
        pc.invView = glm::vec2(1.0f / tex->GetWidth(), 1.0f / tex->GetHeight());

        vkCmdPushConstants(cmd, rr->lightVolumePipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightVolumePushConstants), &pc);

        // Single instanced draw for all local lights
        const MeshInfo& sphereInfo = uMeshes->GetMeshInfo(sphereID);
        vkCmdDrawIndexed(cmd, sphereInfo.indexCount, mLocalLightCount, sphereInfo.firstIndex, sphereInfo.vertexOffset, 0);
    }

    void RenderSystem::RenderToneMapVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        ScopedDebugLabel label(rr->device.get(), cmd, "Tone Map Pass", glm::vec4(0.5f, 0.2f, 0.8f, 1.0f));

        rr->tonemapPipeline->Bind(cmd);
        rr->tonemapUniform->Bind(cmd, rr->tonemapPipeline->GetLayout(), rr->currentFrameIndex);
        SetViewportAndScissor(cmd, rr->swapChain->GetSwapChainExtent());

        // Fullscreen triangle
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }
}
