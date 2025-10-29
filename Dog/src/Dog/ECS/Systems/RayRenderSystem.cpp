#include <PCH/pch.h>
#include "RayRenderSystem.h"

#include "../Resources/renderingResource.h"
#include "../Resources/EditorResource.h"
#include "../Resources/DebugDrawResource.h"

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
#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/Vulkan/Texture/Texture.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Common/UnifiedMesh.h"

#include "../ECS.h"
#include "ECS/Entities/Entity.h"
#include "ECS/Entities/Components.h"

namespace Dog
{
    RayRenderSystem::RayRenderSystem() : ISystem("RayRenderSystem") {}
    RayRenderSystem::~RayRenderSystem() {}

    void RayRenderSystem::Init()
    {
        auto rr = ecs->GetResource<RenderingResource>();

        VmaAllocator allocator = rr->allocator->GetAllocator();
        mIndirectBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
        mIndirectBufferAllocations.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i) {
            VkBufferCreateInfo indirectBufferInfo = {};
            indirectBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            indirectBufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * InstanceUniforms::MAX_INSTANCES;
            indirectBufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

            VmaAllocationCreateInfo indirectAllocInfo = {};
            indirectAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // This usage allows the CPU to write commands

            // Create the buffers using VMA
            if (vmaCreateBuffer(allocator, &indirectBufferInfo, &indirectAllocInfo, &mIndirectBuffers[i], &mIndirectBufferAllocations[i], nullptr) != VK_SUCCESS) 
            {
                throw std::runtime_error("Failed to create solid indirect command buffer!");
            }
        }

        CreateBottomLevelAS();  // Set up BLAS infrastructure
        CreateTopLevelAS();     // Set up TLAS infrastructure
    }
    
    void RayRenderSystem::FrameStart()
    {
        // Update textures!
        ecs->GetResource<RenderingResource>()->UpdateTextureUniform();

        DebugDrawResource::Clear();

        if (InputSystem::isKeyTriggered(Key::Z))
        {
            // mPipeline->Recreate();
        }
    }
    
    void RayRenderSystem::Update(float dt)
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
            std::bind(&RayRenderSystem::RenderScene, this, std::placeholders::_1)
        );
    }
    
    void RayRenderSystem::FrameEnd()
    {
    }

    void RayRenderSystem::Exit()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i) {
            vmaDestroyBuffer(rr->allocator->GetAllocator(), mIndirectBuffers[i], mIndirectBufferAllocations[i]);
        }
    }

    void RayRenderSystem::RenderScene(VkCommandBuffer cmd)
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

        VkDrawIndexedIndirectCommand* indirectDrawPtr;
        vmaMapMemory(rr->allocator->GetAllocator(), mIndirectBufferAllocations[rr->currentFrameIndex], (void**)&indirectDrawPtr);

        int instanceBaseIndex = 0, drawCount = 0;
        mNumInstancesRendered = 0;

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

        //// test calling primitive to geometry
        //VKMesh& testMesh = uMesh;
        //VkAccelerationStructureGeometryKHR geometry{};
        //VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        //PrimitiveToGeometry(testMesh, geometry, rangeInfo);
        //
        //// print stats
        //DOG_INFO("Primitive Count: {}", rangeInfo.primitiveCount);
    }

    void RayRenderSystem::PrimitiveToGeometry(VKMesh& mesh, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
    {
        VkDeviceAddress vertexAddress = mesh.mVertexBuffer->GetDeviceAddress();
        VkDeviceAddress indexAddress = mesh.mIndexBuffer->GetDeviceAddress();

        // Describe buffer as array of VertexObj.
        VkAccelerationStructureGeometryTrianglesDataKHR triangles{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,  // vec3 vertex position data
            .vertexData = {.deviceAddress = vertexAddress},
            .vertexStride = sizeof(Vertex),
            .maxVertex = static_cast<uint32_t>(mesh.mVertices.size() - 1),
            .indexType = VK_INDEX_TYPE_UINT32, 
            .indexData = {.deviceAddress = indexAddress},
        };

        // Identify the above data as containing opaque triangles.
        geometry = VkAccelerationStructureGeometryKHR{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = triangles},
            .flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR | VK_GEOMETRY_OPAQUE_BIT_KHR,
        };

        rangeInfo = VkAccelerationStructureBuildRangeInfoKHR{ .primitiveCount = mesh.mTriangleCount };
    }

    void RayRenderSystem::CreateAccelerationStructure(VkAccelerationStructureTypeKHR asType,  // The type of acceleration structure (BLAS or TLAS)
        AccelerationStructure& accelStruct,  // The acceleration structure to create
        VkAccelerationStructureGeometryKHR& asGeometry,  // The geometry to build the acceleration structure from
        VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,  // The range info for building the acceleration structure
        VkBuildAccelerationStructureFlagsKHR flags  // Build flags (e.g. prefer fast trace)
    )
    {
        auto rr = ecs->GetResource<RenderingResource>();
        VkDevice device = rr->device->GetDevice();
        auto asProps = rr->device->GetAccelerationStructureProperties();

        // Helper function to align a value to a given alignment
        auto alignUp = [](auto value, size_t alignment) noexcept { return ((value + alignment - 1) & ~(alignment - 1)); };

        // Fill the build information with the current information, the rest is filled later (scratch buffer and destination AS)
        VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = asType,  // The type of acceleration structure (BLAS or TLAS)
            .flags = flags,   // Build flags (e.g. prefer fast trace)
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,  // Build mode vs update
            .geometryCount = 1,                                               // Deal with one geometry at a time
            .pGeometries = &asGeometry,  // The geometry to build the acceleration structure from
        };

        // One geometry at a time (could be multiple)
        std::vector<uint32_t> maxPrimCount(1);
        maxPrimCount[0] = asBuildRangeInfo.primitiveCount;

        // Find the size of the acceleration structure and the scratch buffer
        VkAccelerationStructureBuildSizesInfoKHR asBuildSize{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        rr->device->g_vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildInfo,
            maxPrimCount.data(), &asBuildSize);

        // Make sure the scratch buffer is properly aligned
        VkDeviceSize scratchSize = alignUp(asBuildSize.buildScratchSize, asProps.minAccelerationStructureScratchOffsetAlignment);

        // Create the scratch buffer to store the temporary data for the build
        ABuffer scratchBuffer;
        
        rr->allocator->CreateBuffer(scratchBuffer, scratchSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VMA_MEMORY_USAGE_AUTO, asProps.minAccelerationStructureScratchOffsetAlignment);

        // Create the acceleration structure
        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .size = asBuildSize.accelerationStructureSize,  // The size of the acceleration structure
            .type = asType,  // The type of acceleration structure (BLAS or TLAS)
        };
        rr->allocator->CreateAcceleration(accelStruct, createInfo);

        // Build the acceleration structure
        {
            VkCommandBuffer cmd = rr->device->BeginSingleTimeCommands();

            // Fill with new information for the build,scratch buffer and destination AS
            asBuildInfo.dstAccelerationStructure = accelStruct.accel;
            asBuildInfo.scratchData.deviceAddress = scratchBuffer.address;

            VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &asBuildRangeInfo;
            rr->device->g_vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildInfo, &pBuildRangeInfo);

            rr->device->EndSingleTimeCommands(cmd);
        }

        // Cleanup the scratch buffer
        rr->allocator->DestroyBuffer(scratchBuffer);
    }

    void RayRenderSystem::CreateBottomLevelAS()
    {
        uint32_t numMeshes = 0;

        auto rr = ecs->GetResource<RenderingResource>();
        const auto& ml = rr->modelLibrary;
        for (uint32_t i = 0; i < ml->GetModelCount(); ++i)
        {
            const auto& model = ml->GetModel(i);
            numMeshes += (uint32_t)model->mMeshes.size();
        }

        // Prepare geometry information for all meshes
        mBlasAccel.resize(numMeshes);

        // For now, just log that we're ready to build BLAS
        DOG_INFO("Ready to build {} bottom-level acceleration structures", numMeshes);

        // TODO: In Phase 3, we'll add the actual building:
        // For each mesh 
        //   - create acceleration structure geometry from internal mesh primitive (primitiveToGeometry)
        //   - create acceleration structure
    }

    void RayRenderSystem::CreateTopLevelAS()
    {
        // VkTransformMatrixKHR is row-major 3x4, glm::mat4 is column-major; transpose before memcpy.
        auto toTransformMatrixKHR = [](const glm::mat4& m) {
            VkTransformMatrixKHR t;
            memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
            return t;
        };

        // Prepare instance data for TLAS
        std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
        tlasInstances.reserve(mNumInstancesRendered);

        auto rr = ecs->GetResource<RenderingResource>();
        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->TryAddGetModel(mc.ModelPath);
            if (!model) continue;

            auto unifiedMesh = rr->modelLibrary->GetUnifiedMesh();
            for (auto& mesh : model->mMeshes)
            {
                const MeshInfo& meshInfo = unifiedMesh->GetMeshInfo(mesh->GetID());

                VkAccelerationStructureInstanceKHR asInstance{};
                asInstance.transform = toTransformMatrixKHR(tc.GetTransform() * model->GetNormalizationMatrix());  // Position of the instance

                // TODO: Fix the custom instance to match the one in BLAS
                asInstance.instanceCustomIndex = mesh->GetID();                       // gl_InstanceCustomIndexEXT
                // asInstance.accelerationStructureReference = m_blasAccel[instance.meshIndex].address;  // Will be set in Phase 3
                asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
                asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;  // No culling - double sided
                asInstance.mask = 0xFF;
                tlasInstances.emplace_back(asInstance);
            }
        }

        // For now, just log that we're ready to build TLAS
        DOG_INFO("Ready to build top-level acceleration structure with {} instances", tlasInstances.size());

        // TODO: In Phase 3, we'll add the actual building:
        // 1. Create and upload instance buffer
        // 2. Create TLAS geometry from instances
        // 3. Call createAccelerationStructure with TLAS type
    }
}
