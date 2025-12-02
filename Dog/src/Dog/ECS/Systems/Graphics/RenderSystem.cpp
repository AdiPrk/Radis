#include <PCH/pch.h>
#include "RenderSystem.h"

#include "ECS/Resources/renderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/DebugDrawResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/SwapRendererResource.h"
#include "ECS/Resources/RaytracingResource.h"

#include "../InputSystem.h"

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


namespace Dog
{
    RenderSystem::RenderSystem() : ISystem("RenderSystem") {}
    RenderSystem::~RenderSystem() {}

    void RenderSystem::Init()
    {
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

        DebugDrawResource::Clear();
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

        mInstanceData.clear();
        mLightData.clear();
        const auto& debugData = DebugDrawResource::GetInstanceData();
        
        if (!rr->useRaytracing)
        {
            if (mInstanceData.size() + debugData.size() < InstanceUniforms::MAX_INSTANCES)
            {
                mInstanceData.insert(mInstanceData.end(), debugData.begin(), debugData.end());
            }
            else
            {
                DOG_WARN("Too many instances for debug draw render!");
            }
        }

        auto& registry = ecs->GetRegistry();
        registry.view<LightComponent, TransformComponent>().each([&](auto entity, LightComponent& lc, TransformComponent& tc)
        {
            LightUniform lu{};
            lu.position = glm::vec4(tc.Translation, 1.0f);
            lu.radius = lc.Radius;
            lu.color = lc.Color;
            lu.intensity = lc.Intensity;
            lu.direction = glm::normalize(lc.Direction);
            lu.innerCone = lc.InnerCone;
            lu.outerCone = lc.OuterCone;
            lu.type = static_cast<int>(lc.Type);
            lu._padding[0] = 777;
            lu._padding[1] = 777;
            mLightData.push_back(lu);
        });

        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();
        UnifiedMeshes* uMeshes = ml->GetUnifiedMesh();

        uint32_t indexOffset = 0;
        uint32_t vertexOffset = 0;
        registry.view<ModelComponent, TransformComponent>().each([&](auto entity, ModelComponent& mc, TransformComponent& tc)
        {
            Model* model = rr->modelLibrary->TryAddGetModel(mc.ModelPath);
            if (!model) return;

            AnimationComponent* ac = registry.try_get<AnimationComponent>(entity);

            uint32_t boneOffset = AnimationLibrary::INVALID_ANIMATION_INDEX;
            if (ac && al->GetAnimation(ac->AnimationIndex) && al->GetAnimator(ac->AnimationIndex))
            {
                boneOffset = ac->BoneOffset;
            }

            for (auto& mesh : model->mMeshes)
            {
                InstanceUniforms& data = mInstanceData.emplace_back();
                if (boneOffset == AnimationLibrary::INVALID_ANIMATION_INDEX)
                {
                    data.model = tc.GetTransform() * model->GetNormalizationMatrix();
                }
                else
                {
                    data.model = tc.GetTransform();
                }
                
                const MeshInfo& meshInfo = uMeshes->GetMeshInfo(mesh->GetID());
                float meshMetallic = mc.useMetallicOverride ? mc.metallicOverride : mesh->metallicFactor;
                float meshRoughness = mc.useRoughnessOverride ? mc.roughnessOverride : mesh->roughnessFactor;
                uint32_t metallicIndex = mc.useMetallicOverride ? TextureLibrary::INVALID_TEXTURE_INDEX : mesh->metalnessTextureIndex;
                uint32_t roughnessIndex = mc.useRoughnessOverride ? TextureLibrary::INVALID_TEXTURE_INDEX : mesh->roughnessTextureIndex;
                if (mesh->mMetallicRoughnessCombined) roughnessIndex = metallicIndex;

                data.tint = mc.tintColor;
                data.textureIndicies = glm::uvec4(mesh->albedoTextureIndex, mesh->normalTextureIndex, metallicIndex, roughnessIndex);
                data.textureIndicies2 = glm::uvec4(mesh->occlusionTextureIndex, mesh->emissiveTextureIndex, 10001, 10001);
                data.boneOffset = boneOffset;
                data.baseColorFactor = mesh->baseColorFactor;
                data.metallicRoughnessFactor = glm::vec4(meshMetallic, meshRoughness, 0.f, 0.f);
                data.emissiveFactor = mesh->emissiveFactor;
                data.indexOffset = meshInfo.firstIndex;
                data.vertexOffset = meshInfo.vertexOffset;
                data.meshID = mesh->GetID();
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

            // Add the scene render pass
            auto& rg = rr->renderGraph;
            if (Engine::GetEditorEnabled()) 
            {
                if (rr->useRaytracing)
                {
                    rg->AddPass(
                        "ScenePass",
                        [&](RGPassBuilder& builder) {},
                        std::bind(&RenderSystem::RaytraceSceneVK, this, std::placeholders::_1)
                    );
                }
                else
                {
                    rg->AddPass(
                        "ScenePass",
                        [&](RGPassBuilder& builder) 
                        {
                            builder.writes("SceneColor");
                            builder.writes("SceneDepth");
                        },
                        std::bind(&RenderSystem::RenderSceneVK, this, std::placeholders::_1)
                    );
                }
            }
            else
            {
                rg->AddPass(
                    "ScenePass",
                    [&](RGPassBuilder& builder) 
                    {
                        builder.writes("BackBuffer");
                        builder.writes("SceneDepth");
                    },
                    std::bind(&RenderSystem::RenderSceneVK, this, std::placeholders::_1)
                );
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

    void RenderSystem::RenderSceneVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

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

        auto& registry = ecs->GetRegistry();

        UnifiedMeshes* uMeshes = rr->modelLibrary->GetUnifiedMesh();
        uMeshes->GetUnifiedMesh()->Bind(cmd);

        int baseIndex = 0;
        Model* cubeModel = ml->TryAddGetModel("Assets/Models/cube.obj");
        auto& cubeMeshData = uMeshes->GetMeshInfo(cubeModel->mMeshes[0]->GetID());
        
        uint32_t debugDrawCount = DebugDrawResource::GetInstanceDataSize();
        vkCmdDrawIndexed(cmd, cubeMeshData.indexCount, debugDrawCount, cubeMeshData.firstIndex, cubeMeshData.vertexOffset, baseIndex);
        baseIndex += debugDrawCount;
        
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            for (auto& mesh : model->mMeshes)
            {
                auto& meshData = uMeshes->GetMeshInfo(mesh->GetID());
                vkCmdDrawIndexed(cmd, meshData.indexCount, 1, meshData.firstIndex, meshData.vertexOffset, baseIndex);

                ++baseIndex;
            }
        }
    }

    void RenderSystem::RenderSceneGL()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto er = ecs->GetResource<EditorResource>();
        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

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

        int baseIndex = 0;
        Model* cubeModel = ml->GetModel("Assets/Models/cube.obj");
        auto& cubeMeshData = uMeshes->GetMeshInfo(cubeModel->mMeshes[0]->GetID());

        uint32_t debugDrawCount = DebugDrawResource::GetInstanceDataSize();
        glDrawElementsInstancedBaseVertexBaseInstance(
            GL_TRIANGLES,
            static_cast<GLsizei>(cubeMeshData.indexCount),
            GL_UNSIGNED_INT,
            (void*)(sizeof(uint32_t) * cubeMeshData.firstIndex),
            debugDrawCount,
            cubeMeshData.vertexOffset,
            baseIndex
        );
        baseIndex += debugDrawCount;
        
        auto& registry = ecs->GetRegistry();
        registry.view<ModelComponent, TransformComponent>().each([&](auto entityHandle, ModelComponent& mc, TransformComponent& tc)
        {
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) return;

            for (auto& mesh : model->mMeshes)
            {
                auto& meshData = uMeshes->GetMeshInfo(mesh->GetID());
                glDrawElementsInstancedBaseVertexBaseInstance(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(meshData.indexCount),
                    GL_UNSIGNED_INT,
                    (void*)(sizeof(uint32_t) * meshData.firstIndex),
                    1,
                    meshData.vertexOffset,
                    baseIndex
                );

                ++baseIndex;
            }
        });

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
                asInstance.transform = toTransformMatrixKHR(instanceData.model);  // Position of the instance
                asInstance.instanceCustomIndex = static_cast<uint32_t>(i); // gl_InstanceCustomIndexEXT
                asInstance.accelerationStructureReference = rr->blasAccel[instanceData.meshID].address;
                asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
                asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;  // No culling - double sided
                asInstance.mask = 0xFF;
                tlasInstances.emplace_back(asInstance);
            }

            if (tlasInstances.empty())
            {
                // Empty tlas, maybe if we need it
                VkAccelerationStructureInstanceKHR asInstance{};
                asInstance.transform = toTransformMatrixKHR(glm::mat4(0.0f));
                asInstance.instanceCustomIndex = 0;
                asInstance.accelerationStructureReference = rr->blasAccel[0].address;
                asInstance.instanceShaderBindingTableRecordOffset = 0;
                asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                asInstance.mask = 0xFF;
                tlasInstances.emplace_back(asInstance);
            }

            // update!!
            auto rtr = ecs->GetResource<RaytracingResource>();
            rtr->UpdateTopLevelASImmediate(tlasInstances);
        }
        
        // Ray trace pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rp->GetPipeline());

        // Bind the descriptor sets for the graphics pipeline (making textures available to the shaders)
        rr->cameraUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
        rr->rtUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

        // Ray trace
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
