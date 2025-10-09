#include <PCH/pch.h>
#include "RenderSystem.h"

#include "../Resources/renderingResource.h"
#include "../Resources/EditorResource.h"
#include "../Resources/DebugDrawResource.h"

#include "InputSystem.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/Model/ModelLibrary.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"
#include "Graphics/Vulkan/Model/Model.h"
#include "Graphics/Vulkan/Uniform/Uniform.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Model/Animation/AnimationLibrary.h"
#include "Graphics/Vulkan/Texture/TextureLibrary.h"
#include "Graphics/Vulkan/Texture/Texture.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Vulkan/Model/UnifiedMesh.h"

#include "../ECS.h"
#include "ECS/Entities/Entity.h"
#include "ECS/Entities/Components.h"

namespace Dog
{
    RenderSystem::RenderSystem() : ISystem("RenderSystem") {}
    RenderSystem::~RenderSystem() {}

    void RenderSystem::Init()
    {
        auto rr = ecs->GetResource<RenderingResource>();

        std::vector<Uniform*> unis{
            rr->cameraUniform.get(),
            rr->instanceUniform.get()
        };

        mPipeline = std::make_unique<Pipeline>(
            *rr->device,
            rr->swapChain->GetImageFormat(), rr->swapChain->FindDepthFormat(),
            unis,
            false,
            "basic_model.vert", "basic_model.frag"
        );

        mWireframePipeline = std::make_unique<Pipeline>(
            *rr->device,
            rr->swapChain->GetImageFormat(), rr->swapChain->FindDepthFormat(),
            unis,
            true,
            "basic_model.vert", "basic_model.frag"
        );

        VmaAllocator allocator = rr->allocator->GetAllocator();
        mIndirectBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
        mIndirectBufferAllocations.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i) {
            VkBufferCreateInfo indirectBufferInfo = {};
            indirectBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            indirectBufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * 10000;
            indirectBufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

            VmaAllocationCreateInfo indirectAllocInfo = {};
            indirectAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // This usage allows the CPU to write commands

            // Create the buffers using VMA
            if (vmaCreateBuffer(allocator, &indirectBufferInfo, &indirectAllocInfo, &mIndirectBuffers[i], &mIndirectBufferAllocations[i], nullptr) != VK_SUCCESS) 
            {
                throw std::runtime_error("Failed to create solid indirect command buffer!");
            }
        }
    }
    
    void RenderSystem::FrameStart()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto& ml = rr->modelLibrary;
        if (ml->NeedsTextureUpdate())
        {
            ml->LoadTextures();

            auto& tl = rr->textureLibrary;

            size_t textureCount = tl->GetTextureCount();
            std::vector<VkDescriptorImageInfo> imageInfos(TextureLibrary::MAX_TEXTURE_COUNT);

            VkSampler defaultSampler = tl->GetSampler();

            bool hasTex = textureCount > 0;
            for (size_t j = 0; j < TextureLibrary::MAX_TEXTURE_COUNT; ++j) {
                imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfos[j].sampler = defaultSampler;
                imageInfos[j].imageView = hasTex ? tl->GetTextureByIndex(static_cast<uint32_t>(std::min(j, textureCount - 1))).GetImageView() : 0;
            }

            for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex) {
                DescriptorWriter writer(*rr->instanceUniform->GetDescriptorLayout(), *rr->instanceUniform->GetDescriptorPool());
                writer.WriteImage(0, imageInfos.data(), static_cast<uint32_t>(imageInfos.size()));
                writer.Overwrite(rr->instanceUniform->GetDescriptorSets()[frameIndex]);
            }
        }

        DebugDrawResource::Clear();
    }
    
    void RenderSystem::Update(float dt)
    {
        auto renderingResource = ecs->GetResource<RenderingResource>();
        auto editorResource = ecs->GetResource<EditorResource>();
        auto& rg = renderingResource->renderGraph;

        // Set camera uniform!
        {
            CameraUniforms camData{};

            // get camera entity
            Entity cameraEntity = ecs->GetEntity("Camera");
            if (cameraEntity) 
            {
                TransformComponent& tc = cameraEntity.GetComponent<TransformComponent>();
                CameraComponent& cc = cameraEntity.GetComponent<CameraComponent>();
                
                // Get the position directly
                glm::vec3 cameraPos = tc.Translation;

                // Calculate direction vectors from the rotation quaternion
                glm::vec3 forwardDir = glm::normalize(cc.Forward);
                glm::vec3 upDir = glm::normalize(cc.Up);

                // Calculate the target and call lookAt
                glm::vec3 cameraTarget = cameraPos + forwardDir;
                camData.view = glm::lookAt(cameraPos, cameraTarget, upDir);
            }
            else
            {
                camData.view = glm::mat4(1.0f);
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
            std::bind(&RenderSystem::RenderScene, this, std::placeholders::_1)
        );
    }
    
    void RenderSystem::FrameEnd()
    {
    }

    void RenderSystem::Exit()
    {
        mPipeline.reset();
        mWireframePipeline.reset();

        auto rr = ecs->GetResource<RenderingResource>();
        
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i) {
            vmaDestroyBuffer(rr->allocator->GetAllocator(), mIndirectBuffers[i], mIndirectBufferAllocations[i]);
        }
    }

    void RenderSystem::RenderScene(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();

        rr->renderWireframe ? mWireframePipeline->Bind(cmd) : mPipeline->Bind(cmd);
        VkPipelineLayout pipelineLayout = rr->renderWireframe ? mWireframePipeline->GetLayout() : mPipeline->GetLayout();

        rr->cameraUniform->Bind(cmd, pipelineLayout, rr->currentFrameIndex);
        rr->instanceUniform->Bind(cmd, pipelineLayout, rr->currentFrameIndex);

        VkViewport viewport{};
        viewport.width = static_cast<float>(rr->swapChain->GetSwapChainExtent().width);
        viewport.height = static_cast<float>(rr->swapChain->GetSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, rr->swapChain->GetSwapChainExtent() };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        std::vector<InstanceUniforms> instanceData{};

        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();

        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

        UnifiedMeshes* unifiedMesh = rr->modelLibrary->GetUnifiedMesh();
        Mesh& uMesh = unifiedMesh->GetUnifiedMesh();

        VkDrawIndexedIndirectCommand* solidPtr;
        vmaMapMemory(rr->allocator->GetAllocator(), mIndirectBufferAllocations[rr->currentFrameIndex], (void**)&solidPtr);

        int drawIndex = 0, instanceBaseIndex = 0, solidDrawCount = 0;

        const auto& debugData = DebugDrawResource::GetInstanceData();
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
            drawCommand.instanceCount = debugData.size();
            drawCommand.firstIndex = meshInfo.firstIndex;
            drawCommand.vertexOffset = meshInfo.vertexOffset;
            drawCommand.firstInstance = instanceBaseIndex;
            solidPtr[solidDrawCount++] = drawCommand;

            instanceBaseIndex += debugData.size();
        }

        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            AnimationComponent* ac = entity.HasComponent<AnimationComponent>() ? &entity.GetComponent<AnimationComponent>() : nullptr;
            Model* model = rr->modelLibrary->TryAddGetModel(mc.ModelPath);
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
                    data.model = tc.GetTransform() * model->mNormalizationMatrix;
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
                solidPtr[solidDrawCount++] = drawCommand;
            }
        }

        vmaUnmapMemory(rr->allocator->GetAllocator(), mIndirectBufferAllocations[rr->currentFrameIndex]);


        rr->instanceUniform->SetUniformData(instanceData, 1, rr->currentFrameIndex);
        
        VkBuffer uMeshVertexBuffer = uMesh.mVertexBuffer->GetBuffer();
        VkBuffer uMeshIndexBuffer = uMesh.mIndexBuffer->GetBuffer();
        VkBuffer instBuffer = rr->instanceUniform->GetUniformBuffer(1, rr->currentFrameIndex)->GetBuffer();
        VkBuffer buffers[] = { uMeshVertexBuffer, instBuffer };
        VkDeviceSize offsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);
        vkCmdBindIndexBuffer(cmd, uMeshIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(
            cmd,
            mIndirectBuffers[rr->currentFrameIndex],
            0,
            static_cast<uint32_t>(solidDrawCount),
            sizeof(VkDrawIndexedIndirectCommand)
        );
    }

    void RenderSystem::RenderSkeleton()
    {
        // loop over entities with models
        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();

        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            //AnimationComponent* ac = entity.HasComponent<AnimationComponent>() ? &entity.GetComponent<AnimationComponent>() : nullptr;
            Model* model = ecs->GetResource<RenderingResource>()->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            glm::mat4 normalizationMatrix = model->mNormalizationMatrix;
            //if (!ac) normalizationMatrix = glm::mat4(1.f);

            const aiScene* scene = model->mScene;
            RecursiveNodeDraw(tc.GetTransform() * normalizationMatrix, scene->mRootNode);
        }
    }

    void RenderSystem::RecursiveNodeDraw(const glm::mat4& parentWorldTransform, const aiNode* node)
    {
        glm::mat4 localTr = aiMatToGlm(node->mTransformation);
        glm::mat4 worldTr = parentWorldTransform * localTr;

        glm::vec3 startPos = glm::vec3(parentWorldTransform[3]);
        glm::vec3 endPos = glm::vec3(worldTr[3]);

        if (node->mParent && node->mParent->mParent != nullptr)
        {
            DebugDrawResource::DrawLine(startPos, endPos, glm::vec4(1.f, 0.f, 1.f, 1.f));
            DebugDrawResource::DrawCube(endPos, glm::vec3(0.01f), glm::vec4(0.f, 1.f, 1.f, 0.4f));
        }


        // Recurse :3
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            RecursiveNodeDraw(worldTr, node->mChildren[i]);
        }
    }
}
