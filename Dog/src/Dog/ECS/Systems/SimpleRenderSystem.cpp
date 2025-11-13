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
#include "Graphics/Vulkan/Pipeline/RaytracingPipeline.h"
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
        auto rr = ecs->GetResource<RenderingResource>();

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan && !rr->tlasAccel.accel && rr->blasAccel.empty())
        {
            // get num entities with model component
            mConstStartingObjectCount = 0;
            auto view = ecs->GetRegistry().view<ModelComponent>();
            mRTMeshData.clear();
            mRTMeshIndices.clear();
            ecs->GetRegistry().view<ModelComponent>().each([&](auto entity, ModelComponent& mc) 
            {
                auto model = rr->modelLibrary->GetModel(mc.ModelPath);
                if (model)
                {
                    for (auto& mesh : model->mMeshes)
                    {
                        mConstStartingObjectCount++;

                        MeshDataUniform vertexData;
                        for (auto& v : mesh->mVertices)
                        {
                            vertexData.position = v.position;
                            vertexData.color = v.color;
                            vertexData.normal = v.normal;
                            vertexData.texCoord = v.uv;
                            vertexData.boneIds = glm::ivec4(v.boneIDs[0], v.boneIDs[1], v.boneIDs[2], v.boneIDs[3]);
                            vertexData.weights = glm::vec4(v.weights[0], v.weights[1], v.weights[2], v.weights[3]);
                            mRTMeshData.push_back(vertexData);
                        }

                        for (auto& index : mesh->mIndices)
                        {
                            mRTMeshIndices.push_back(index);
                        }
                    }
                }
            });

            CreateBottomLevelAS();  // Set up BLAS infrastructure
            CreateTopLevelAS();     // Set up TLAS infrastructure
            
            VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
            asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            asInfo.accelerationStructureCount = 1;
            asInfo.pAccelerationStructures = &rr->tlasAccel.accel;
            for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
            {
                DescriptorWriter writer(*rr->rtUniform->GetDescriptorLayout(), *rr->rtUniform->GetDescriptorPool());
                writer.WriteAccelerationStructure(1, &asInfo);
                writer.Overwrite(rr->rtUniform->GetDescriptorSets()[frameIndex]);
            }

            rr->rtUniform->SetUniformData(mRTMeshData, 2, 0);
            rr->rtUniform->SetUniformData(mRTMeshData, 2, 1);
            rr->rtUniform->SetUniformData(mRTMeshIndices, 3, 0);
            rr->rtUniform->SetUniformData(mRTMeshIndices, 3, 1);
        }

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
        if (InputSystem::isKeyTriggered(Key::T))
        {
            auto sr = ecs->GetResource<SwapRendererBackendResource>();
            sr->RequestSwap();
        }
        // static int si = 0;
        // if (si++ % 30 == 0) 
        // {
        //     auto sr = ecs->GetResource<SwapRendererBackendResource>();
        //     sr->RequestSwap();
        // }
    }

    void SimpleRenderSystem::Update(float dt)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        float aspectRatio = 1.f;
        if (Engine::GetEditorEnabled())
        {
            auto editorResource = ecs->GetResource<EditorResource>();
            aspectRatio = editorResource->sceneWindowWidth / editorResource->sceneWindowHeight;
        }
        else
        {
            auto windowResource = ecs->GetResource<WindowResource>();
            auto extant = windowResource->window->GetExtent();
            aspectRatio = static_cast<float>(extant.x) / static_cast<float>(extant.y);
        }

        mNumObjectsRendered = 0;

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

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto& rg = rr->renderGraph;
            rr->cameraUniform->SetUniformData(camData, 0, rr->currentFrameIndex);

            // Add the scene render pass
            if (Engine::GetEditorEnabled()) 
            {
                if (rr->useRaytracing)
                {
                    rg->AddPass(
                        "ScenePass",
                        [&](RGPassBuilder& builder) {},
                        std::bind(&SimpleRenderSystem::RaytraceScene, this, std::placeholders::_1)
                    );
                }
                else
                {
                    rg->AddPass(
                        "ScenePass",
                        [&](RGPassBuilder& builder) {
                            builder.writes("SceneColor");
                            builder.writes("SceneDepth");
                        },
                        std::bind(&SimpleRenderSystem::RenderSceneVK, this, std::placeholders::_1)
                    );
                }
                
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
            //rr->shader->SetViewAndProjectionView(camData.view, camData.projectionView);
            rr->shader->SetCameraUBO(camData);

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

        VkViewport viewport{};
        viewport.width = static_cast<float>(rr->swapChain->GetSwapChainExtent().width);
        viewport.height = static_cast<float>(rr->swapChain->GetSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, rr->swapChain->GetSwapChainExtent() };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        std::vector<InstanceUniforms> instanceData{};
        std::vector<LightUniform> lightData{};
        auto debugData = DebugDrawResource::GetInstanceData();
        //auto lightTestData = DebugDrawResource::CreateDebugLightTest();
        //debugData.insert(debugData.end(), lightTestData.begin(), lightTestData.end());

        if (instanceData.size() + debugData.size() >= InstanceUniforms::MAX_INSTANCES)
        {
            DOG_WARN("Too many instances for debug draw render!");
        }
        else
        {
            //instanceData.insert(instanceData.end(), debugData.begin(), debugData.end());            
        }

        AnimationLibrary* al = rr->animationLibrary.get();
        ModelLibrary* ml = rr->modelLibrary.get();

        auto& registry = ecs->GetRegistry();

        ecs->GetRegistry().view<LightComponent>().each([&](auto entity, LightComponent& lc)
        {
            Entity lightEntity(&registry, entity);
            TransformComponent& tc = lightEntity.GetComponent<TransformComponent>();

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
            lightData.push_back(lu);
        });

        auto entityView = registry.view<ModelComponent, TransformComponent>();
        uint32_t indexOffset = 0;
        uint32_t vertexOffset = 0;
        for (auto& entityHandle : entityView)
        {
            Entity entity(&registry, entityHandle);
            ModelComponent& mc = entity.GetComponent<ModelComponent>();
            TransformComponent& tc = entity.GetComponent<TransformComponent>();
            Model* model = rr->modelLibrary->TryAddGetModel(mc.ModelPath);
            AnimationComponent* ac = entity.TryGetComponent<AnimationComponent>();
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
                data.textureIndicies = glm::uvec4(mesh->albedoTextureIndex, mesh->normalTextureIndex, mesh->metalnessTextureIndex, mesh->roughnessTextureIndex);
                data.textureIndicies2 = glm::uvec4(mesh->occlusionTextureIndex, mesh->emissiveTextureIndex, 10001, 10001);
                data.boneOffset = boneOffset;
                data.baseColorFactor = mesh->baseColorFactor;
                data.metallicRoughnessFactor = glm::vec4(mesh->metallicFactor, mesh->roughnessFactor, 0.f, 0.f);
                data.emissiveFactor = mesh->emissiveFactor;

                data.indexOffset = indexOffset;
                data.vertexOffset = vertexOffset;
                indexOffset += static_cast<uint32_t>(mesh->mIndices.size());
                vertexOffset += static_cast<uint32_t>(mesh->mVertices.size());

                data._padding = 777;
            }
        }

        rr->cameraUniform->SetUniformData(instanceData, 1, rr->currentFrameIndex);


        struct Header {
            uint32_t lightCount;
            uint32_t _pad[3];
        };

        Header header = { static_cast<uint32_t>(lightData.size()), {0,0,0} };

        std::vector<uint8_t> gpuBuffer(sizeof(Header) + sizeof(LightUniform) * lightData.size());
        memcpy(gpuBuffer.data(), &header, sizeof(Header));
        memcpy(gpuBuffer.data() + sizeof(Header), lightData.data(), sizeof(LightUniform) * lightData.size());
        rr->cameraUniform->SetUniformData(gpuBuffer, 4, rr->currentFrameIndex);

        VkBuffer instBuffer = rr->cameraUniform->GetUniformBuffer(1, rr->currentFrameIndex).buffer;
        int baseIndex = 0;
        //for (auto& data : debugData)
        //{
        //    Model* cubeModel = ml->TryAddGetModel("Assets/Models/cube.obj");
        //    auto& mesh = cubeModel->mMeshes[0];
        //    mesh->Bind(cmd, instBuffer);
        //    mesh->Draw(cmd, baseIndex++);
        //
        //    ++mNumObjectsRendered;
        //}

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
                data.textureIndicies = glm::uvec4(mesh->albedoTextureIndex, mesh->normalTextureIndex, mesh->metalnessTextureIndex, mesh->roughnessTextureIndex);
                data.textureIndicies2 = glm::uvec4(mesh->occlusionTextureIndex, mesh->emissiveTextureIndex, 10001, 10001);
                data.boneOffset = boneOffset;
                data.baseColorFactor = mesh->baseColorFactor;
                data.metallicRoughnessFactor = glm::vec4(mesh->metallicFactor, mesh->roughnessFactor, 0.f, 0.f);
                data.emissiveFactor = mesh->emissiveFactor;
                data._padding = 777.0f;
            }
        }

        uint32_t instanceCount = static_cast<uint32_t>(instanceData.size());
        for (uint32_t i = 0; i < rr->textureLibrary->GetTextureCount(); ++i)
        {
            auto itex = rr->textureLibrary->GetTextureByIndex(i);
            if (itex)
            {
                GLTexture* gltex = static_cast<GLTexture*>(itex);
                textureData.push_back(gltex->textureHandle);
            }
        }

        GLShader::SetupInstanceSSBO();
        GLShader::SetupTextureSSBO();

        GLuint iSSBO = GLShader::GetInstanceSSBO();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, iSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, instanceCount * sizeof(InstanceUniforms), instanceData.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        
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

        DOG_INFO("Base instance index: {}", baseIndex);

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

        Allocator::CreateBuffer(
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
        Allocator::SetAllocationName(scratchBuffer.allocation, "AS Scratch Buffer");

        // Create the acceleration structure
        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .size = asBuildSize.accelerationStructureSize,  // The size of the acceleration structure
            .type = asType,  // The type of acceleration structure (BLAS or TLAS)
        };
        Allocator::CreateAcceleration(accelStruct, createInfo);
        Allocator::SetAllocationName(accelStruct.buffer.allocation, (asType & VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR) ? "Bottom Level AS" : "Top Level AS");

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
        Allocator::DestroyBuffer(scratchBuffer);
    }

    void SimpleRenderSystem::CreateBottomLevelAS()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        uint32_t numMeshes = 0;

        const auto& ml = rr->modelLibrary;
        for (uint32_t i = 0; i < ml->GetModelCount(); ++i)
        {
            const auto& model = ml->GetModel(i);
            numMeshes += (uint32_t)model->mMeshes.size();
        }

        // Prepare geometry information for all meshes
        rr->blasAccel.resize(numMeshes);

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
                CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, rr->blasAccel[mesh->GetID()], asGeometry,
                    asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
            }
        }
    }

    void SimpleRenderSystem::CreateTopLevelAS()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        // VkTransformMatrixKHR is row-major 3x4, glm::mat4 is column-major; transpose before memcpy.
        auto toTransformMatrixKHR = [](const glm::mat4& m) {
            VkTransformMatrixKHR t;
            memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
            return t;
        };

        // Prepare instance data for TLAS
        std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
        tlasInstances.reserve(mConstStartingObjectCount);

        auto& registry = ecs->GetRegistry();
        auto entityView = registry.view<ModelComponent, TransformComponent>();
        int instanceIndex = 0;
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
                asInstance.instanceCustomIndex = instanceIndex++;//mesh->GetID();                       // gl_InstanceCustomIndexEXT
                asInstance.accelerationStructureReference = rr->blasAccel[mesh->GetID()].address;
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
            if (bufferSize == 0)
            {
                DOG_WARN("No TLAS instances to build!");
                return;
            }

            // Create host-visible staging buffer
            Allocator::CreateBuffer(
                stagingBuffer,
                bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY
            );
            Allocator::SetAllocationName(stagingBuffer.allocation, "TLAS Instance Staging Buffer");

            // Upload instance data
            void* data;
            vmaMapMemory(Allocator::GetAllocator(), stagingBuffer.allocation, &data);
            memcpy(data, tlasInstances.data(), bufferSize);
            vmaUnmapMemory(Allocator::GetAllocator(), stagingBuffer.allocation);

            // Create GPU-local buffer for TLAS build input
            Allocator::CreateBuffer(
                tlasInstancesBuffer,
                bufferSize,
                VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY
            );
            Allocator::SetAllocationName(tlasInstancesBuffer.allocation, "TLAS Instance Buffer");

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
                rr->tlasAccel,
                asGeometry,
                asBuildRangeInfo, 
                VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
            );
            Allocator::SetAllocationName(rr->tlasAccel.buffer.allocation, "Top Level AS");

            if (rr->device->g_vkSetDebugUtilsObjectNameEXT)
            {
                VkAccelerationStructureKHR accelToName = rr->tlasAccel.accel;
                const char* bufferName = "TLAS Accel"; // Your custom name

                VkDebugUtilsObjectNameInfoEXT nameInfo = {};
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.pNext = nullptr;

                nameInfo.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
                nameInfo.objectHandle = (uint64_t)accelToName;
                nameInfo.pObjectName = bufferName;

                rr->device->g_vkSetDebugUtilsObjectNameEXT(rr->device->GetDevice(), &nameInfo);
            }
        }

        DOG_INFO("  Top-level acceleration structures built successfully\n");

        // Cleanup staging and instance buffers
        Allocator::DestroyBuffer(stagingBuffer);
        Allocator::DestroyBuffer(tlasInstancesBuffer);
    }

    void SimpleRenderSystem::RaytraceScene(VkCommandBuffer cmd)
    {
        auto rr = ecs->GetResource<RenderingResource>();

        VkDebugUtilsLabelEXT s{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, "Raytrace Scene", {1.0f, 1.0f, 1.0f, 1.0f}};
        if (rr->device->g_vkCmdBeginDebugUtilsLabelEXT) 
        {
            rr->device->g_vkCmdBeginDebugUtilsLabelEXT(cmd, &s);
        }
        auto& rp = rr->raytracingPipeline;
        
        // Ray trace pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rp->GetPipeline());

        // Bind the descriptor sets for the graphics pipeline (making textures available to the shaders)
        rr->cameraUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
        rr->rtUniform->Bind(cmd, rp->GetLayout(), rr->currentFrameIndex, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

        // Ray trace
        const VkExtent2D& size = rr->swapChain->GetSwapChainExtent();
        rr->device->g_vkCmdTraceRaysKHR(cmd, &rp->GetRaygenRegion(), &rp->GetMissRegion(), &rp->GetHitRegion(), &rp->GetCallableRegion(), size.width, size.height, 1);

        // Barrier to make sure the image is ready for Tonemapping
        // don't have this function; write the memory barrier ourselves;
        //nvvk::cmdMemoryBarrier(cmd, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
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

        if (rr->device->g_vkCmdEndDebugUtilsLabelEXT)
        {
            rr->device->g_vkCmdEndDebugUtilsLabelEXT(cmd);
        }
    }
}
