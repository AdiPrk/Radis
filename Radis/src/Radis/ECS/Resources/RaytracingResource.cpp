#include <PCH/pch.h>
#include "RaytracingResource.h"
#include "RenderingResource.h"

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
#include "Graphics/Vulkan/VKMesh.h"

#include "ECS/ECS.h"
#include "ECS/Components/Components.h"
#include "ECS/Entities/Entity.h"

namespace Radis
{
    RaytracingResource::RaytracingResource()
    {
    }

    void RaytracingResource::PrimitiveToGeometry(VKMesh& mesh, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
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
        // @Todo: Future optimization: Check if the material has any transparency and set VK_GEOMETRY_OPAQUE_BIT_KHR
        // to skip the any-hit shader
        geometry = VkAccelerationStructureGeometryKHR{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = triangles},
            .flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
        };


        rangeInfo = VkAccelerationStructureBuildRangeInfoKHR{ .primitiveCount = mesh.mTriangleCount };
    }

    void RaytracingResource::CreateAccelerationStructure(VkAccelerationStructureTypeKHR asType,  // The type of acceleration structure (BLAS or TLAS)
        AccelerationStructure& accelStruct,  // The acceleration structure to create
        VkAccelerationStructureGeometryKHR& asGeometry,  // The geometry to build the acceleration structure from
        VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,  // The range info for building the acceleration structure
        VkBuildAccelerationStructureFlagsKHR flags,  // Build flags (e.g. prefer fast trace)
        AccelerationStructure const* srcAccel, // Optional source AS for updates
        VkBuildAccelerationStructureModeKHR buildMode // Build mode (build or update)
    )
    {
        auto rr = ecs->GetResource<RenderingResource>();
        VkDevice device = rr->device->GetDevice();
        const auto& asProps = rr->device->GetAccelerationStructureProperties();

        // Helper function to align a value to a given alignment
        auto alignUp = [](auto value, size_t alignment) noexcept { return ((value + alignment - 1) & ~(alignment - 1)); };

        // Fill the build information with the current information, the rest is filled later (scratch buffer and destination AS)
        VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = asType,  // The type of acceleration structure (BLAS or TLAS)
            .flags = flags,   // Build flags (e.g. prefer fast trace)
            .mode = buildMode,  // Build mode vs update
            .srcAccelerationStructure = srcAccel ? srcAccel->accel : VK_NULL_HANDLE, // Source AS for updates
            .geometryCount = 1,                                               // Deal with one geometry at a time
            .pGeometries = &asGeometry,  // The geometry to build the acceleration structure from
        };

        // One geometry at a time (could be multiple)
        std::vector<uint32_t> maxPrimCount(1);
        maxPrimCount[0] = asBuildRangeInfo.primitiveCount;

        // Find the size of the acceleration structure and the scratch buffer
        VkAccelerationStructureBuildSizesInfoKHR asBuildSize{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildInfo,
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
            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildInfo, &pBuildRangeInfo);

            rr->device->EndSingleTimeCommands(cmd);
        }

        // Cleanup the scratch buffer
        Allocator::DestroyBuffer(scratchBuffer);
    }

    void RaytracingResource::CreateBLAS()
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
                CreateAccelerationStructure(
                    VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                    rr->blasAccel[mesh->GetID()],
                    asGeometry,
                    asBuildRangeInfo,
                    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                );
                Allocator::SetAllocationName(rr->blasAccel[mesh->GetID()].buffer.allocation, "Bottom Level AS Mesh");
            }
        }
    }

    void RaytracingResource::CreateTLAS()
    {
        auto rr = ecs->GetResource<RenderingResource>();

        // Prepare instance data for TLAS
        std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
        tlasInstances.reserve(64);

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

                asInstance.instanceCustomIndex = instanceIndex++;//mesh->GetID();  // gl_InstanceCustomIndexEXT
                asInstance.accelerationStructureReference = rr->blasAccel[mesh->GetID()].address;
                asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
                asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;  // No culling - double sided
                asInstance.mask = 0xFF;
                tlasInstances.emplace_back(asInstance);
            }
        }

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

            // Copy data from staging device-local buffer
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
                .primitiveCount = static_cast<uint32_t>(tlasInstances.size())
            };

            CreateAccelerationStructure(
                VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                rr->tlasAccel,
                asGeometry,
                asBuildRangeInfo,
                VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
            );
            Allocator::SetAllocationName(rr->tlasAccel.buffer.allocation, "Top Level AS");

            if (vkSetDebugUtilsObjectNameEXT)
            {
                VkAccelerationStructureKHR accelToName = rr->tlasAccel.accel;
                const char* bufferName = "TLAS Accel"; // Your custom name

                VkDebugUtilsObjectNameInfoEXT nameInfo = {};
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.pNext = nullptr;

                nameInfo.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
                nameInfo.objectHandle = (uint64_t)accelToName;
                nameInfo.pObjectName = bufferName;

                vkSetDebugUtilsObjectNameEXT(rr->device->GetDevice(), &nameInfo);
            }
        }

        DOG_INFO("Top-level AS built with {} instances!", tlasInstances.size());

        // Cleanup staging and instance buffers
        Allocator::DestroyBuffer(stagingBuffer);
        Allocator::DestroyBuffer(tlasInstancesBuffer);
    }

    void RaytracingResource::UpdateTopLevelASImmediate(const std::vector<VkAccelerationStructureInstanceKHR>& instances)
    {
        auto rr = ecs->GetResource<RenderingResource>();
        if (instances.empty())
        {
            DOG_WARN("UpdateTopLevelASImmediate: no instances to build/update.");
            return;
        }

        VkDeviceSize bufferSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

        // --- Create staging buffer (CPU visible) ---
        Buffer stagingBuffer;
        Allocator::CreateBuffer(
            stagingBuffer,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
        Allocator::SetAllocationName(stagingBuffer.allocation, "TLAS Instance Staging Buffer");

        // Upload instance array into staging buffer
        {
            void* mapped = nullptr;
            vmaMapMemory(Allocator::GetAllocator(), stagingBuffer.allocation, &mapped);
            memcpy(mapped, instances.data(), static_cast<size_t>(bufferSize));
            vmaUnmapMemory(Allocator::GetAllocator(), stagingBuffer.allocation);
        }

        // --- Create device-local instance buffer ---
        Buffer tlasInstancesBuffer;
        Allocator::CreateBuffer(
            tlasInstancesBuffer,
            bufferSize,
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        Allocator::SetAllocationName(tlasInstancesBuffer.allocation, "TLAS Instance Buffer");

        // Begin single-time command buffer (this submits+waits inside EndSingleTimeCommands)
        VkCommandBuffer cmd = rr->device->BeginSingleTimeCommands();

        // Copy staging -> device-local
        {
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = bufferSize;
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, tlasInstancesBuffer.buffer, 1, &copyRegion);
        }

        // --- Prepare TLAS geometry and range ---
        VkAccelerationStructureGeometryKHR       asGeometry{};
        VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
        VkAccelerationStructureGeometryInstancesDataKHR geometryInstances{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data = {.deviceAddress = tlasInstancesBuffer.address }
        };

        asGeometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {.instances = geometryInstances },
        };

        asBuildRangeInfo = { .primitiveCount = static_cast<uint32_t>(instances.size()) };

        // Build info set for "build" first to query sizes
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = 1,
            .pGeometries = &asGeometry,
        };

        std::vector<uint32_t> maxPrimCount(1);
        maxPrimCount[0] = asBuildRangeInfo.primitiveCount;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(
            rr->device->GetDevice(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            maxPrimCount.data(),
            &sizeInfo);

        // Create scratch buffer of aligned size
        const auto& asProps = rr->device->GetAccelerationStructureProperties();
        VkDeviceSize scratchSize = sizeInfo.buildScratchSize;
        // align scratch to minAccelerationStructureScratchOffsetAlignment
        scratchSize = (scratchSize + asProps.minAccelerationStructureScratchOffsetAlignment - 1) &
            ~((VkDeviceSize)asProps.minAccelerationStructureScratchOffsetAlignment - 1);
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
        Allocator::SetAllocationName(scratchBuffer.allocation, "TLAS Scratch Buffer");

        // Decide whether we can UPDATE in-place or must build a new TLAS
        bool doUpdate = false;
        if (rr->tlasAccel.accel != VK_NULL_HANDLE)
        {
            // Only allow in-place UPDATE when:
            // 1) the AS buffer is big enough, and
            // 2) the instance count (primitiveCount) exactly matches the last build.
            if (rr->tlasAccel.buffer.bufferSize >= sizeInfo.accelerationStructureSize &&
                rr->tlasAccel.instanceCount == asBuildRangeInfo.primitiveCount)
            {
                doUpdate = true;
            }
            else
            {
                doUpdate = false;
            }
        }

        VkAccelerationStructureKHR dstAS = VK_NULL_HANDLE;
        AccelerationStructure newAccel{}; // temp if we create a new one

        if (doUpdate)
        {
            buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
            buildInfo.srcAccelerationStructure = rr->tlasAccel.accel;
            buildInfo.dstAccelerationStructure = rr->tlasAccel.accel;
        }
        else
        {
            // Create a fresh acceleration structure sized for this TLAS
            VkAccelerationStructureCreateInfoKHR createInfo{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .size = sizeInfo.accelerationStructureSize,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            };

            Allocator::CreateAcceleration(newAccel, createInfo);
            Allocator::SetAllocationName(newAccel.buffer.allocation, "Top Level AS (recreated)");

            buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildInfo.srcAccelerationStructure = rr->tlasAccel.accel ? rr->tlasAccel.accel : VK_NULL_HANDLE;
            buildInfo.dstAccelerationStructure = newAccel.accel;

            dstAS = newAccel.accel;
        }

        // Set scratch device address
        buildInfo.scratchData.deviceAddress = scratchBuffer.address;

        const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &asBuildRangeInfo;

        // Record build/update command into the same command buffer
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);

        // Ensure build finished (we are still in single-time command; but add a barrier just in case)
        // Use a memory barrier that signals AS write -> AS read (so next shaders can read it)
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, 0,
            1, &barrier, 0, nullptr, 0, nullptr);

        // End single-time commands (this submits and waits)
        rr->device->EndSingleTimeCommands(cmd);

        // Because EndSingleTimeCommands waits, we can now safely destroy staging and scratch buffers
        Allocator::DestroyBuffer(stagingBuffer);
        Allocator::DestroyBuffer(tlasInstancesBuffer);
        Allocator::DestroyBuffer(scratchBuffer);

        // If we created a new TLAS, replace rr->tlasAccel and update descriptor sets
        if (!doUpdate)
        {
            // Destroy old TLAS buffers (if any)
            if (rr->tlasAccel.accel != VK_NULL_HANDLE)
            {
                // Destroy the old AS buffer and handle
                Allocator::DestroyAcceleration(rr->tlasAccel);
            }

            // Move newAccel into rr->tlasAccel
            rr->tlasAccel = newAccel;
            rr->tlasAccel.instanceCount = asBuildRangeInfo.primitiveCount;

            // Optionally set debug name
            if (vkSetDebugUtilsObjectNameEXT)
            {
                VkAccelerationStructureKHR accelToName = rr->tlasAccel.accel;
                const char* bufferName = "TLAS Accel";
                VkDebugUtilsObjectNameInfoEXT nameInfo = {};
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
                nameInfo.objectHandle = (uint64_t)accelToName;
                nameInfo.pObjectName = bufferName;
                vkSetDebugUtilsObjectNameEXT(rr->device->GetDevice(), &nameInfo);
            }

            // Update the acceleration-structure descriptor with the new handle (per-frame)
            VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
            asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            asWrite.accelerationStructureCount = 1;
            asWrite.pAccelerationStructures = &rr->tlasAccel.accel;

            DescriptorWriter writer(*rr->rtUniform->GetDescriptorLayout(), *rr->rtUniform->GetDescriptorPool());
            writer.WriteAccelerationStructure(0, &asWrite);
            for (int frameIndex = 0; frameIndex < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
            {
                writer.Overwrite(rr->rtUniform->GetDescriptorSets()[frameIndex]);
            }
        }
    }
}
