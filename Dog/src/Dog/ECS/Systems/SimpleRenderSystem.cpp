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
#include "Graphics/Common/Animation/AnimationLibrary.h"
#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/Vulkan/Texture/VKTexture.h"
#include "Graphics/Vulkan/Uniform/Descriptors.h"
#include "Graphics/Common/UnifiedMesh.h"

#include "../ECS.h"
#include "ECS/Entities/Entity.h"
#include "ECS/Entities/Components.h"

#include "Engine.h"
#include "Graphics/OpenGL/GLMesh.h"
#include "Graphics/OpenGL/GLFrameBuffer.h"
#include "Graphics/OpenGL/GLTexture.h"

namespace Dog
{
    SimpleRenderSystem::SimpleRenderSystem() : ISystem("SimpleRenderSystem") {}
    SimpleRenderSystem::~SimpleRenderSystem() {}

    void SimpleRenderSystem::Init()
    {
    }

    void SimpleRenderSystem::Exit()
    {
        auto& allocator = ecs->GetResource<RenderingResource>()->allocator;
        if (!allocator)
            return;

        for (auto& blas : mBlasAccel)
        {
            allocator->DestroyAcceleration(blas);
        }
        allocator->DestroyAcceleration(mTlasAccel);
    }

    /*********************************************************************
     * param:  gridSize: Number of lines in each direction from the center (total lines = gridSize * 2 + 1)
     * param:  step: Distance between each grid line
     *
     * brief:  Draws a standard editor grid on the XZ plane (Y=0).
     *********************************************************************/
    void DrawEditorGrid(int gridSize = 50, float step = 1.0f)
    {
        // Define colors
        glm::vec4 gridColor(0.4f, 0.4f, 0.4f, 0.75f);   // A medium grey
        glm::vec4 xAxisColor(1.0f, 0.0f, 0.0f, 0.75f);  // Red
        glm::vec4 zAxisColor(0.0f, 0.0f, 1.0f, 0.75f);  // Blue

        float fGridSize = (float)gridSize * step;

        // Draw lines parallel to the Z-axis (varying x)
        for (int i = -gridSize; i <= gridSize; ++i)
        {
            float x = (float)i * step;
            glm::vec3 start(x, 0.0f, -fGridSize);
            glm::vec3 end(x, 0.0f, fGridSize);

            // Use Z-axis color if x is 0, otherwise use regular grid color
            glm::vec4 color = (i == 0) ? zAxisColor : gridColor;

            DebugDrawResource::DrawLine(start, end, color);
        }

        // Draw lines parallel to the X-axis (varying z)
        for (int i = -gridSize; i <= gridSize; ++i)
        {
            float z = (float)i * step;
            glm::vec3 start(-fGridSize, 0.0f, z);
            glm::vec3 end(fGridSize, 0.0f, z);

            // Use X-axis color if z is 0, otherwise use regular grid color
            glm::vec4 color = (i == 0) ? xAxisColor : gridColor;

            DebugDrawResource::DrawLine(start, end, color);
        }
    }

    void SimpleRenderSystem::FrameStart()
    {
        // static bool createdAS = false;
        // if (!createdAS && Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        // {
        //     CreateBottomLevelAS();  // Set up BLAS infrastructure
        //     CreateTopLevelAS();     // Set up TLAS infrastructure
        //     createdAS = true;
        // }

        // Update textures!
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            ecs->GetResource<RenderingResource>()->UpdateTextureUniform();
        }

        DebugDrawResource::Clear();

        DrawEditorGrid(50, 1.0f);

        if (InputSystem::isKeyTriggered(Key::Z))
        {
            // mPipeline->Recreate();
        }

        // Heh
        // static int si = 0;
        // if (si++ % 2 == 0) 
        // {
        //     auto sr = ecs->GetResource<SwapRendererBackendResource>();
        //     sr->RequestSwap();
        // }
    }

    void SimpleRenderSystem::Update(float dt)
    {
        mNumObjectsRendered = 0;

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            float aspectRatio = static_cast<float>(rr->swapChain->GetWidth()) / static_cast<float>(rr->swapChain->GetHeight());
            if (Engine::GetEditorEnabled())
            {
                auto editorResource = ecs->GetResource<EditorResource>();
                aspectRatio = editorResource->sceneWindowWidth / editorResource->sceneWindowHeight;
            }
            auto& rg = rr->renderGraph;

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

                camData.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
                camData.projection[1][1] *= -1;
                camData.projectionView = camData.projection * camData.view;
                rr->cameraUniform->SetUniformData(camData, 0, rr->currentFrameIndex);
            }

            // Add the scene render pass
            if (Engine::GetEditorEnabled()) {
                rg->AddPass(
                    "ScenePass",
                    [&](RGPassBuilder& builder) {
                        builder.writes("SceneColor");
                        builder.writes("SceneDepth");
                    },
                    std::bind(&SimpleRenderSystem::RenderSceneVK, this, std::placeholders::_1)
                );
            }
            else
            {
                rg->AddPass(
                    "ScenePass",
                    [&](RGPassBuilder& builder) {
                        builder.writes("BackBuffer");
                        builder.writes("SceneDepth");
                    },
                    std::bind(&SimpleRenderSystem::RenderSceneVK, this, std::placeholders::_1)
                );
            }
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            RenderSceneGL();
        }
    }

    void SimpleRenderSystem::FrameEnd()
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

        std::vector<InstanceUniforms> instanceData{};
        const auto& debugData = DebugDrawResource::GetInstanceData();
        if (instanceData.size() + debugData.size() >= InstanceUniforms::MAX_INSTANCES)
        {
            DOG_WARN("Too many instances for debug draw render!");
        }
        else
        {
            instanceData.insert(instanceData.end(), debugData.begin(), debugData.end());            
        }

        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->TryAddGetModel(mc.ModelPath);
            AnimationComponent* ac = entity.HasComponent<AnimationComponent>() ? &entity.GetComponent<AnimationComponent>() : nullptr;
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
                    data.model = tc.GetTransform()/* * model->GetNormalizationMatrix()*/;
                }
                else
                {
                    data.model = tc.GetTransform();
                }

                data.tint = mc.tintColor;
                data.textureIndex = mesh->diffuseTextureIndex;
                data.boneOffset = boneOffset;
            }
        }

        rr->cameraUniform->SetUniformData(instanceData, 1, rr->currentFrameIndex);

        VkBuffer instBuffer = rr->cameraUniform->GetUniformBuffer(1, rr->currentFrameIndex).buffer;
        int baseIndex = 0;
        for (auto& data : debugData)
        {
            Model* cubeModel = ml->GetModel("Assets/Models/cube.obj");
            auto& mesh = cubeModel->mMeshes[0];
            mesh->Bind(cmd, instBuffer);
            mesh->Draw(cmd, baseIndex++);

            ++mNumObjectsRendered;
        }

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

                ++mNumObjectsRendered;
            }
        }
    }

    void SimpleRenderSystem::RenderSceneGL()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto er = ecs->GetResource<EditorResource>();

        rr->shader->Use();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        rr->sceneFrameBuffer->Bind();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        std::vector<InstanceUniforms> instanceData{};
        std::vector<uint64_t> textureData{};
        const auto& debugData = DebugDrawResource::GetInstanceData();
        if (instanceData.size() + debugData.size() >= InstanceUniforms::MAX_INSTANCES)
        {
            DOG_WARN("Too many instances for debug draw render!");
        }
        else
        {
            instanceData.insert(instanceData.end(), debugData.begin(), debugData.end());
        }

        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            AnimationComponent* ac = entity.HasComponent<AnimationComponent>() ? &entity.GetComponent<AnimationComponent>() : nullptr;
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
                data.textureIndex = mesh->diffuseTextureIndex;
                data.boneOffset = boneOffset;
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
        glBufferData(GL_ARRAY_BUFFER, instanceCount * sizeof(InstanceUniforms), instanceData.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        for (uint32_t i = 0; i < rr->textureLibrary->GetTextureCount(); ++i)
        {
            auto itex = rr->textureLibrary->GetTextureByIndex(i);
            if (itex)
            {
                GLTexture* gltex = static_cast<GLTexture*>(itex);
                textureData.push_back(gltex->textureHandle);
            }
        }
        
        GLShader::SetupTextureSSBO();
        GLuint textureSSBO = GLShader::GetTextureSSBO();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, textureData.size() * sizeof(uint64_t), textureData.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        int baseIndex = 0;
        for (auto& data : debugData)
        {
            Model* cubeModel = ml->GetModel("Assets/Models/cube.obj");
            auto& mesh = cubeModel->mMeshes[0];
            mesh->Bind(nullptr, nullptr);
            mesh->Draw(nullptr, baseIndex++);

            ++mNumObjectsRendered;
        }

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

                ++mNumObjectsRendered;
            }
        }

        rr->sceneFrameBuffer->Unbind();
    }


    void SimpleRenderSystem::PrimitiveToGeometry(VKMesh& mesh, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
    {
        VkDeviceAddress vertexAddress = mesh.mVertexBuffer.address;
        VkDeviceAddress indexAddress = mesh.mIndexBuffer.address;

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

    void SimpleRenderSystem::CreateAccelerationStructure(VkAccelerationStructureTypeKHR asType,  // The type of acceleration structure (BLAS or TLAS)
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
        Buffer scratchBuffer;

        rr->allocator->CreateBuffer(
            scratchBuffer,
            scratchSize, 
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | 
            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | 
            VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            VMA_MEMORY_USAGE_AUTO, 
            {},
            asProps.minAccelerationStructureScratchOffsetAlignment
        );

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

    void SimpleRenderSystem::CreateBottomLevelAS()
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

        for (uint32_t i = 0; i < ml->GetModelCount(); ++i)
        {
            for (auto& mesh : ml->GetModel(i)->mMeshes)
            {
                VKMesh* vkMesh = static_cast<VKMesh*>(mesh.get());
                VkAccelerationStructureGeometryKHR       asGeometry{};
                VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

                // Convert the primitive information to acceleration structure geometry
                PrimitiveToGeometry(*vkMesh, asGeometry, asBuildRangeInfo);
                CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, mBlasAccel[mesh->GetID()], asGeometry,
                    asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
            }
        }
    }

    void SimpleRenderSystem::CreateTopLevelAS()
    {
        // VkTransformMatrixKHR is row-major 3x4, glm::mat4 is column-major; transpose before memcpy.
        auto toTransformMatrixKHR = [](const glm::mat4& m) {
            VkTransformMatrixKHR t;
            memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
            return t;
        };

        // Prepare instance data for TLAS
        std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
        tlasInstances.reserve(mConstStartingObjectCount);

        auto rr = ecs->GetResource<RenderingResource>();
        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model) continue;

            auto unifiedMesh = rr->modelLibrary->GetUnifiedMesh();
            for (auto& mesh : model->mMeshes)
            {
                VkAccelerationStructureInstanceKHR asInstance{};
                asInstance.transform = toTransformMatrixKHR(tc.GetTransform() * model->GetNormalizationMatrix());  // Position of the instance

                // TODO: Fix the custom instance to match the one in BLAS
                asInstance.instanceCustomIndex = mesh->GetID();                       // gl_InstanceCustomIndexEXT
                asInstance.accelerationStructureReference = mBlasAccel[mesh->GetID()].address;
                asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
                asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;  // No culling - double sided
                asInstance.mask = 0xFF;
                tlasInstances.emplace_back(asInstance);
            }
        }

        // For now, just log that we're ready to build TLAS
        DOG_INFO("Ready to build top-level acceleration structure with {} instances", tlasInstances.size());

        // Then create the buffer with the instance data
        // --- Stage & Copy TLAS instance data ---
        Buffer stagingBuffer;
        Buffer tlasInstancesBuffer;

        {
            VkDeviceSize bufferSize = std::span<VkAccelerationStructureInstanceKHR const>(tlasInstances).size_bytes();

            // Create host-visible staging buffer
            rr->allocator->CreateBuffer(
                stagingBuffer,
                bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY
            );

            // Upload instance data
            void* data;
            vmaMapMemory(rr->allocator->GetAllocator(), stagingBuffer.allocation, &data);
            memcpy(data, tlasInstances.data(), bufferSize);
            vmaUnmapMemory(rr->allocator->GetAllocator(), stagingBuffer.allocation);

            // Create GPU-local buffer for TLAS build input
            rr->allocator->CreateBuffer(
                tlasInstancesBuffer,
                bufferSize,
                VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY
            );

            // Copy data from staging ¨ device-local buffer
            VkCommandBuffer cmd = rr->device->BeginSingleTimeCommands();

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = bufferSize;
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, tlasInstancesBuffer.buffer, 1, &copyRegion);

            rr->device->EndSingleTimeCommands(cmd);
        }

        // Then create the TLAS geometry
        {
            VkAccelerationStructureGeometryKHR       asGeometry{};
            VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

            // Convert the instance information to acceleration structure geometry, similar to primitiveToGeometry()
            VkAccelerationStructureGeometryInstancesDataKHR geometryInstances{ 
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = {.deviceAddress = tlasInstancesBuffer.address} 
            };

            asGeometry = { 
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
                .geometry = {.instances = geometryInstances} 
            };
            
            asBuildRangeInfo = { 
                .primitiveCount = static_cast<uint32_t>(mConstStartingObjectCount) 
            };

            CreateAccelerationStructure(
                VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                mTlasAccel,
                asGeometry,
                asBuildRangeInfo, 
                VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
            );

            if (rr->device->g_vkSetDebugUtilsObjectNameEXT)
            {
                // 1. Define your object and its desired name
                VkAccelerationStructureKHR accelToName = mTlasAccel.accel;
                const char* bufferName = "TLAS Accel"; // Your custom name

                // 2. Fill the info struct
                VkDebugUtilsObjectNameInfoEXT nameInfo = {};
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.pNext = nullptr;

                // 3. Specify the object type
                nameInfo.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;

                // 4. Cast your Vulkan handle to a uint64_t
                nameInfo.objectHandle = (uint64_t)accelToName;

                // 5. Set the pointer to your C-string name
                nameInfo.pObjectName = bufferName;

                // 6. Make the call
                rr->device->g_vkSetDebugUtilsObjectNameEXT(rr->device->GetDevice(), &nameInfo);
            }
        }

        DOG_INFO("  Top-level acceleration structures built successfully\n");

        // Cleanup staging and instance buffers
        rr->allocator->DestroyBuffer(stagingBuffer);
        rr->allocator->DestroyBuffer(tlasInstancesBuffer);
    }
}
