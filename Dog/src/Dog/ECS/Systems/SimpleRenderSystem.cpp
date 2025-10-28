#include <PCH/pch.h>
#include "SimpleRenderSystem.h"

#include "../Resources/renderingResource.h"
#include "../Resources/EditorResource.h"
#include "../Resources/DebugDrawResource.h"
#include "../Resources/WindowResource.h"
#include "../Resources/SwapRendererBackendResource.h"

#include "InputSystem.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"
#include "Graphics/Common/Model.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Model/Animation/AnimationLibrary.h"
#include "Graphics/Vulkan/Texture/TextureLibrary.h"
#include "Graphics/Vulkan/Texture/Texture.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Common/UnifiedMesh.h"

#include "../ECS.h"
#include "ECS/Entities/Entity.h"
#include "ECS/Entities/Components.h"

#include "Engine.h"
#include "Graphics/OpenGL/GLMesh.h"
#include "Graphics/OpenGL/GLFrameBuffer.h"

namespace Dog
{
    SimpleRenderSystem::SimpleRenderSystem() : ISystem("SimpleRenderSystem") {}
    SimpleRenderSystem::~SimpleRenderSystem() {}

    void SimpleRenderSystem::Init()
    {
    }

    void SimpleRenderSystem::FrameStart()
    {
        // Update textures!
        //ecs->GetResource<RenderingResource>()->UpdateTextureUniform();

        DebugDrawResource::Clear();

        if (InputSystem::isKeyTriggered(Key::Z))
        {
            // mPipeline->Recreate();
        }

        static int si = 0;
        if (si++ % 2 == 0) return;

        auto sr = ecs->GetResource<SwapRendererBackendResource>();
        //sr->RequestSwap();
    }

    void SimpleRenderSystem::Update(float dt)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto renderingResource = ecs->GetResource<RenderingResource>();
            auto editorResource = ecs->GetResource<EditorResource>();
            auto& rg = renderingResource->renderGraph;

            // Set camera uniform!
            {
                CameraUniforms camData{};
                camData.view = glm::mat4(1.0f);

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
                }

                camData.projection = glm::perspective(glm::radians(45.0f), editorResource->sceneWindowWidth / editorResource->sceneWindowHeight, 0.1f, 100.0f);
                camData.projection[1][1] *= -1;
                camData.projectionView = camData.projection * camData.view;
                renderingResource->cameraUniform->SetUniformData(camData, 0, renderingResource->currentFrameIndex);
            }

            // Add the scene render pass
            rg->AddPass(
                "ScenePass",
                [&](RGPassBuilder& builder) {
                    builder.writes("SceneColor");
                    builder.writes("SceneDepth");
                },
                std::bind(&SimpleRenderSystem::RenderSceneVK, this, std::placeholders::_1)
            );
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            RenderSceneGL();
        }
    }

    void SimpleRenderSystem::FrameEnd()
    {
    }

    void SimpleRenderSystem::Exit()
    {
    }

    void SimpleRenderSystem::RenderSceneVK(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();

        rr->renderWireframe ? rr->wireframePipeline->Bind(cmd) : rr->pipeline->Bind(cmd);
        VkPipelineLayout pipelineLayout = rr->renderWireframe ? rr->wireframePipeline->GetLayout() : rr->pipeline->GetLayout();

        rr->cameraUniform->Bind(cmd, pipelineLayout, rr->currentFrameIndex);
        //rr->instanceUniform->Bind(cmd, pipelineLayout, rr->currentFrameIndex);

        VkViewport viewport{};
        viewport.width = static_cast<float>(rr->swapChain->GetSwapChainExtent().width);
        viewport.height = static_cast<float>(rr->swapChain->GetSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, rr->swapChain->GetSwapChainExtent() };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        std::vector<SimpleInstanceUniforms> instanceData{};

        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

        UnifiedMeshes* unifiedMesh = rr->modelLibrary->GetUnifiedMesh();
        VKMesh& uMesh = unifiedMesh->GetUnifiedMesh();

        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            for (auto& mesh : model->mMeshes)
            {
                auto& data = instanceData.emplace_back();
                data.model = tc.GetTransform() * model->GetNormalizationMatrix();
            }
        }

        rr->cameraUniform->SetUniformData(instanceData, 1, rr->currentFrameIndex);

        VkBuffer instBuffer = rr->cameraUniform->GetUniformBuffer(1, rr->currentFrameIndex)->GetBuffer();
        int baseIndex = 0;
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            for (auto& mesh : model->mMeshes)
            {
                mesh->Bind(cmd, instBuffer);
                mesh->Draw(cmd, baseIndex++);
            }
        }

        // VkDrawIndexedIndirectCommand* indirectDrawPtr;
        // vmaMapMemory(rr->allocator->GetAllocator(), mIndirectBufferAllocations[rr->currentFrameIndex], (void**)&indirectDrawPtr);
        // 
        // int instanceBaseIndex = 0, drawCount = 0;
        // mNumInstancesRendered = 0;

        // TODO: NO DEBUG DRAW FOR NOW!
        /*const auto& debugData = DebugDrawResource::GetInstanceData();
        if (instanceData.size() + debugData.size() >= InstanceUniforms::MAX_INSTANCES)
        {
            DOG_WARN("Too many instances for debug draw render!");
        }
        else
        {
            instanceData.insert(instanceData.end(), debugData.begin(), debugData.end());

            auto cubeModel = ml->GetModel("Assets/models/cube.obj");
            const MeshInfo& meshInfo = unifiedMesh->GetMeshInfo(cubeModel->mMeshes[0].mMeshID);

            VkDrawIndexedIndirectCommand drawCommand = {};
            drawCommand.indexCount = meshInfo.indexCount;
            drawCommand.instanceCount = (uint32_t)debugData.size();
            drawCommand.firstIndex = meshInfo.firstIndex;
            drawCommand.vertexOffset = meshInfo.vertexOffset;
            drawCommand.firstInstance = instanceBaseIndex;
            indirectDrawPtr[drawCount++] = drawCommand;

            instanceBaseIndex += (int)debugData.size();
        }*/

        /*
        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            AnimationComponent* ac = entity.HasComponent<AnimationComponent>() ? &entity.GetComponent<AnimationComponent>() : nullptr;
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            uint32_t boneOffset = AnimationLibrary::INVALID_ANIMATION_INDEX;
            if (ac && al->GetAnimation(ac->AnimationIndex) && al->GetAnimator(ac->AnimationIndex))
            {
                boneOffset = ac->BoneOffset;
            }

            for (auto& mesh : model->mMeshes)
            {
                InstanceUniforms& data = instanceData.emplace_back();
                if (boneOffset == AnimationLibrary::INVALID_ANIMATION_INDEX)
                {
                    data.model = tc.GetTransform() * model->GetNormalizationMatrix();
                }
                else
                {
                    data.model = tc.GetTransform();
                }

                data.tint = mc.tintColor;
                data.textureIndex = mesh.diffuseTextureIndex;
                data.boneOffset = boneOffset;

                // Prepare indirect draw command
                const MeshInfo& meshInfo = unifiedMesh->GetMeshInfo(mesh.mMeshID);
                VkDrawIndexedIndirectCommand drawCommand = {};
                drawCommand.indexCount = meshInfo.indexCount;
                drawCommand.instanceCount = 1;
                drawCommand.firstIndex = meshInfo.firstIndex;
                drawCommand.vertexOffset = meshInfo.vertexOffset;
                drawCommand.firstInstance = instanceBaseIndex++;
                indirectDrawPtr[drawCount++] = drawCommand;
                ++mNumInstancesRendered;
            }
        }
        */

        /*
        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            for (auto& mesh : model->mMeshes)
            {
                SimpleInstanceUniforms& data = instanceData.emplace_back();
                data.model = tc.GetTransform() * model->GetNormalizationMatrix();

                // Prepare indirect draw command
                const MeshInfo& meshInfo = unifiedMesh->GetMeshInfo(mesh->GetID());
                VkDrawIndexedIndirectCommand drawCommand = {};
                drawCommand.indexCount = meshInfo.indexCount;
                drawCommand.instanceCount = 1;
                drawCommand.firstIndex = meshInfo.firstIndex;
                drawCommand.vertexOffset = meshInfo.vertexOffset;
                drawCommand.firstInstance = instanceBaseIndex++;
                indirectDrawPtr[drawCount++] = drawCommand;
                ++mNumInstancesRendered;
            }
        }

        vmaUnmapMemory(rr->allocator->GetAllocator(), mIndirectBufferAllocations[rr->currentFrameIndex]);


        //rr->instanceUniform->SetUniformData(instanceData, 1, rr->currentFrameIndex);
        rr->cameraUniform->SetUniformData(instanceData, 1, rr->currentFrameIndex);

        VkBuffer uMeshVertexBuffer = uMesh.mVertexBuffer->GetBuffer();
        VkBuffer uMeshIndexBuffer = uMesh.mIndexBuffer->GetBuffer();
        VkBuffer instBuffer = rr->cameraUniform->GetUniformBuffer(1, rr->currentFrameIndex)->GetBuffer();
        VkBuffer buffers[] = { uMeshVertexBuffer, instBuffer };
        VkDeviceSize offsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);
        vkCmdBindIndexBuffer(cmd, uMeshIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(
            cmd,
            mIndirectBuffers[rr->currentFrameIndex],
            0,
            static_cast<uint32_t>(drawCount),
            sizeof(VkDrawIndexedIndirectCommand)
        );
        */

        //// test calling primitive to geometry
        //VKMesh& testMesh = uMesh;
        //VkAccelerationStructureGeometryKHR geometry{};
        //VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        //PrimitiveToGeometry(testMesh, geometry, rangeInfo);
        //
        //// print stats
        //DOG_INFO("Primitive Count: {}", rangeInfo.primitiveCount);
    }

    void SimpleRenderSystem::RenderSceneGL()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto er = ecs->GetResource<EditorResource>();

        rr->shader->Use();

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        rr->sceneFrameBuffer->Bind();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        std::vector<SimpleInstanceUniforms> instanceData{};

        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            for (auto& mesh : model->mMeshes)
            {
                auto& data = instanceData.emplace_back();
                data.model = tc.GetTransform() * model->GetNormalizationMatrix();
            }
        }

        // Set camera uniform!
        {
            CameraUniforms camData{};
            camData.view = glm::mat4(1.0f);

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
            }

            camData.projection = glm::perspective(glm::radians(45.0f), er->sceneWindowWidth / er->sceneWindowHeight, 0.1f, 100.0f);
            camData.projectionView = camData.projection * camData.view;

            rr->shader->SetViewAndProjectionView(camData.view, camData.projectionView);
        }

        uint32_t instanceCount = static_cast<uint32_t>(instanceData.size());
        
        GLuint iVBO = GLShader::GetInstanceVBO();
        glBindBuffer(GL_ARRAY_BUFFER, iVBO);
        glBufferData(GL_ARRAY_BUFFER, instanceCount * sizeof(SimpleInstanceUniforms),
            instanceData.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        int baseIndex = 0;
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            for (auto& mesh : model->mMeshes)
            {
                mesh->Bind(nullptr, nullptr);
                mesh->Draw(nullptr, baseIndex++);
            }
        }

        rr->sceneFrameBuffer->Unbind();
    }

}
