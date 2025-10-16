#pragma once

#include "ISystem.h"
#include "Graphics/Vulkan/Core/AccelerationStructures.h"

namespace Dog
{
    class Pipeline;
    class Mesh;

    class RayRenderSystem : public ISystem
    {
    public:
        RayRenderSystem();
        ~RayRenderSystem();

        void Init();
        void FrameStart();
        void Update(float dt);
        void FrameEnd();
        void Exit();

        void RenderScene(VkCommandBuffer cmd);

        void PrimitiveToGeometry(Mesh& mesh,
            VkAccelerationStructureGeometryKHR& geometry,
            VkAccelerationStructureBuildRangeInfoKHR& rangeInfo);

        void CreateAccelerationStructure(VkAccelerationStructureTypeKHR asType, AccelerationStructure& accelStruct, VkAccelerationStructureGeometryKHR& asGeometry, VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo, VkBuildAccelerationStructureFlagsKHR flags);
        void CreateBottomLevelAS();
        void CreateTopLevelAS();

    private:
        // Accleration stuffs
        std::vector<AccelerationStructure> mBlasAccel; // Bottom Level Acceleration Structures
        uint32_t mNumInstancesRendered = 0;

    private:
        std::unique_ptr<Pipeline> mPipeline;
        std::unique_ptr<Pipeline> mWireframePipeline;

        std::vector<VkBuffer> mIndirectBuffers;
        std::vector<VmaAllocation> mIndirectBufferAllocations;
    };
}

