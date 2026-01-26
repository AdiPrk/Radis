#include <PCH/pch.h>
#include "RenderSystem.h"

#include "ECS/Resources/renderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/DebugDrawResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/SwapRendererResource.h"
#include "ECS/Resources/RaytracingResource.h"

#include "../InputSystem.h"

#include "Graphics/Vulkan/Uniform/UniformData.h"
#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"
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
#include "Graphics/OpenGL/GLMesh.h"
#include "Graphics/OpenGL/GLFrameBuffer.h"
#include "Graphics/OpenGL/GLTexture.h"

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
                for (auto& v : uMeshes->GetUnifiedMesh()->mVertices)
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
            
                mRTMeshIndices = uMeshes->GetUnifiedMesh()->mIndices;
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

        DebugDrawResource::DrawEditorGrid(50, 1.0f);

        // Heh
        if (InputSystem::isKeyTriggered(Key::R) && InputSystem::isKeyDown(Key::LEFTCONTROL))
        {
            ecs->GetResource<SwapRendererResource>()->RequestSwap();
        }
    }

    void RenderSystem::Update(float dt)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        float aspectRatio = GetAspectRatio();

        CameraUniforms camData{};
        camData.view = glm::mat4(1.0f);
        camData.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f); 

        // get camera entity
        Entity cameraEntity = ecs->GetEntity("Camera");
        if (cameraEntity)
        {
            TransformComponent& tc = cameraEntity.GetComponent<TransformComponent>();
            CameraComponent& cc = cameraEntity.GetComponent<CameraComponent>();

            // Get the position directly
            glm::vec3 cameraPos = tc.Translation;
            glm::vec3 forwardDir = glm::normalize(cc.Forward);
            glm::vec3 upDir = glm::normalize(cc.Up);

            glm::vec3 cameraTarget = cameraPos + forwardDir;
            camData.view = glm::lookAt(cameraPos, cameraTarget, upDir);
            camData.cameraPos = glm::vec4(tc.Translation, 1.0f);
            camData.projection = glm::perspective(glm::radians(cc.FOV), aspectRatio, cc.Near, cc.Far);
        }

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) camData.projection[1][1] *= -1;
        camData.projectionView = camData.projection * camData.view;
        camData.inverseProjView = glm::inverse(camData.projectionView);

        // Collect light data
        mLightData.clear();
        auto& registry = ecs->GetRegistry();
        registry.view<LightComponent, TransformComponent>().each([&](auto entity, LightComponent& lc, TransformComponent& tc)
        {
            // Fun little pattern for light1000 scene
            // float tx += glm::sin(static_cast<float>(totaltime + (uint32_t)entity)) * 0.01f;
            // float ty += glm::cos(static_cast<float>(totaltime + (uint32_t)entity)) * 0.01f;
            // float tz += glm::sin(static_cast<float>(totaltime + (uint32_t)entity)) * 0.02f;
            // tc.SetTranslation(tx, ty, tz);

            LightUniform lu{};
            lu.positionRadius = glm::vec4(tc.Translation, lc.Radius);
            lu.colorIntensity = glm::vec4(lc.Color, lc.Intensity);
            lu.directionInner = glm::vec4(glm::normalize(lc.Direction), lc.InnerCone);
            lu.outerConeType = glm::vec4(lc.OuterCone, static_cast<float>(lc.Type), 0.0f, 0.0f);
            mLightData.push_back(lu);
        });

        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();
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

        // Pass 1: Count instances per mesh
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

                if (boneOffset == AnimationLibrary::INVALID_ANIMATION_INDEX)
                {
                    data.model = tc.GetTransform() * model->GetNormalizationMatrix();
                }
                else
                {
                    data.model = tc.GetTransform();
                }
                
                const MeshInfo& meshInfo = uMeshes->GetMeshInfo(meshID);
                float meshMetallic = mc.useMetallicOverride ? mc.metallicOverride : mesh->metallicFactor;
                float meshRoughness = mc.useRoughnessOverride ? mc.roughnessOverride : mesh->roughnessFactor;
                glm::vec4 meshEmissive = mc.useEmissiveOverride ? mc.emissiveOverride : mesh->emissiveFactor;

                uint32_t metallicIndex = mc.useMetallicOverride ? TextureLibrary::INVALID_TEXTURE_INDEX : mesh->metalnessTextureIndex;
                uint32_t roughnessIndex = mc.useRoughnessOverride ? TextureLibrary::INVALID_TEXTURE_INDEX : mesh->roughnessTextureIndex;
                if (mesh->mMetallicRoughnessCombined) roughnessIndex = metallicIndex;

                data.tint = mc.tintColor;
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

        struct LightHeader { uint32_t lightCount; uint32_t _pad[3]; };
        LightHeader header{ .lightCount = static_cast<uint32_t>(mLightData.size()) };
        mLightBuffer.resize(sizeof(LightHeader) + sizeof(LightUniform) * mLightData.size());
        memcpy(mLightBuffer.data(), &header, sizeof(LightHeader));
        memcpy(mLightBuffer.data() + sizeof(LightHeader), mLightData.data(), sizeof(LightUniform) * mLightData.size());

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            rr->cameraUniform->SetUniformData(camData, 0, rr->currentFrameIndex);       // Set Camera Data
            rr->cameraUniform->SetUniformData(mInstanceData, 1, rr->currentFrameIndex); // Set Instance Data
            rr->cameraUniform->SetUniformData(mLightBuffer, 4, rr->currentFrameIndex);  // Set Light Data

            if (rr->deferredLightingUniform)
            {
                rr->deferredLightingUniform->SetUniformData(camData, 0, rr->currentFrameIndex);      // Camera data
                rr->deferredLightingUniform->SetUniformData(mLightBuffer, 6, rr->currentFrameIndex); // Light data
            }

            // Add the scene render pass
            auto& rg = rr->renderGraph;
            std::string colorWriteTarget = Engine::GetEditorEnabled() ? "SceneTexture" : "BackBuffer";

            switch (rr->renderMode)
            {
            case RenderMode::Forward: {
                rg->AddPass(
                    "ScenePass",
                    [&](RGPassBuilder& builder)
                    {
                        builder.writes(colorWriteTarget);
                        builder.writes("SceneDepth");
                    },
                    std::bind(&RenderSystem::RenderSceneVK, this, std::placeholders::_1)
                );
                break;
            }
            case RenderMode::Deferred: {
                // G-Buffer pass
                rg->AddPass("GBufferPass",
                    [&](RGPassBuilder& builder) {
                        builder.writes("gAlbedo");
                        builder.writes("gNormal");
                        builder.writes("gPBR");
                        builder.writes("gEmissive");
                        builder.writes("SceneDepth");
                    },
                    std::bind(&RenderSystem::RenderSceneDeferredGeometryVK, this, std::placeholders::_1)
                );

                // Lighting pass - outputs to SceneHDR
                rg->AddPass("LightingPass",
                    [&](RGPassBuilder& builder) {
                        builder.reads("gAlbedo");
                        builder.reads("gNormal");
                        builder.reads("gPBR");
                        builder.reads("gEmissive");
                        builder.reads("SceneDepth");
                        builder.writes(colorWriteTarget);
                    },
                    std::bind(&RenderSystem::RenderSceneDeferredLightingVK, this, std::placeholders::_1)
                );

                break;
            }
            case RenderMode::Raytracing: {
                rg->AddPass(
                    "ScenePass",
                    [&](RGPassBuilder& builder) {},
                    std::bind(&RenderSystem::RaytraceSceneVK, this, std::placeholders::_1)
                );
                break;
            }
            }
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            rr->shader->SetCameraUBO(camData);
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

        ScopedDebugLabel sceneDebugLabel(rr->device.get(), cmd, "Render Scene", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

        rr->renderWireframe ? rr->wireframePipeline->Bind(cmd) : rr->pipeline->Bind(cmd);
        VkPipelineLayout pipelineLayout = rr->renderWireframe ? rr->wireframePipeline->GetLayout() : rr->pipeline->GetLayout();

        rr->cameraUniform->Bind(cmd, pipelineLayout, rr->currentFrameIndex);

        VkViewport viewport{};
        viewport.width = static_cast<float>(rr->swapChain->GetSwapChainExtent().width);
        viewport.height = static_cast<float>(rr->swapChain->GetSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, rr->swapChain->GetSwapChainExtent() };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        UnifiedMeshes* uMeshes = rr->modelLibrary->GetUnifiedMesh();
        uMeshes->GetUnifiedMesh()->Bind(cmd);

        ExecuteInstancedDrawCalls(cmd);
    }

    void RenderSystem::RenderSceneDeferredGeometryVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();

        ScopedDebugLabel sceneDebugLabel(rr->device.get(), cmd, "Deferred G-Buffer Pass", glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));

        rr->gBufferPipeline->Bind(cmd);
        VkPipelineLayout pipelineLayout = rr->gBufferPipeline->GetLayout();

        rr->cameraUniform->Bind(cmd, pipelineLayout, rr->currentFrameIndex);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(rr->swapChain->GetSwapChainExtent().width);
        viewport.height = static_cast<float>(rr->swapChain->GetSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, rr->swapChain->GetSwapChainExtent() };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        UnifiedMeshes* uMeshes = rr->modelLibrary->GetUnifiedMesh();
        uMeshes->GetUnifiedMesh()->Bind(cmd);

        ExecuteInstancedDrawCalls(cmd);
    }

    void RenderSystem::RenderSceneDeferredLightingVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();

        ScopedDebugLabel sceneDebugLabel(rr->device.get(), cmd, "Deferred Lighting Pass", glm::vec4(0.8f, 0.8f, 0.2f, 1.0f));

        rr->deferredLightingPipeline->Bind(cmd);
        VkPipelineLayout pipelineLayout = rr->deferredLightingPipeline->GetLayout();

        rr->deferredLightingUniform->Bind(cmd, pipelineLayout, rr->currentFrameIndex);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(rr->swapChain->GetSwapChainExtent().width);
        viewport.height = static_cast<float>(rr->swapChain->GetSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, rr->swapChain->GetSwapChainExtent() };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdDraw(cmd, 3, 1, 0, 0);
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
        uMeshes->GetUnifiedMesh()->Bind();

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
                asInstance.transform = toTransformMatrixKHR(instanceData.model);
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
                asInstance.transform = toTransformMatrixKHR(glm::mat4(0.0f));
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
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rp->GetPipeline());

        rr->cameraUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
        rr->rtUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

        const VkExtent2D& size = rr->swapChain->GetSwapChainExtent();
        vkCmdTraceRaysKHR(cmd, &rp->GetRaygenRegion(), &rp->GetMissRegion(), &rp->GetHitRegion(), &rp->GetCallableRegion(), size.width, size.height, 1);

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
}
