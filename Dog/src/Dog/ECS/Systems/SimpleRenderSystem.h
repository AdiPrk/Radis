#pragma once

#include "ISystem.h"
#include "Graphics/Vulkan/Core/AccelerationStructures.h"

namespace Dog
{
    class SimpleRenderSystem : public ISystem
    {
    public:
        SimpleRenderSystem();
        ~SimpleRenderSystem();

        void Init();
        void FrameStart();
        void Update(float dt);
        void FrameEnd();
        void Exit();

        void RenderSceneVK(VkCommandBuffer cmd);
        void RenderSceneGL();

        // Accel structure functions
        void PrimitiveToGeometry(class VKMesh& mesh,
            VkAccelerationStructureGeometryKHR& geometry,
            VkAccelerationStructureBuildRangeInfoKHR& rangeInfo);

        void CreateAccelerationStructure(VkAccelerationStructureTypeKHR asType, AccelerationStructure& accelStruct, VkAccelerationStructureGeometryKHR& asGeometry, VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo, VkBuildAccelerationStructureFlagsKHR flags);
        void CreateBottomLevelAS();
        void CreateTopLevelAS();

    private:
        std::vector<AccelerationStructure> mBlasAccel; // Bottom Level Acceleration Structures
        AccelerationStructure mTlasAccel;              // Top Level Acceleration Structure
        uint32_t mNumObjectsRendered = 0;
        const uint32_t mConstStartingObjectCount = 1;

    };
}

