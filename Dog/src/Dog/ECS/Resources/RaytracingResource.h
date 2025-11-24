#pragma once

#include "IResource.h"
#include "Graphics/Vulkan/Core/AccelerationStructures.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

namespace Dog
{
    struct RaytracingResource : public IResource
    {
        RaytracingResource();

        // Accel structure functions
        void PrimitiveToGeometry(class VKMesh& mesh,
            VkAccelerationStructureGeometryKHR& geometry,
            VkAccelerationStructureBuildRangeInfoKHR& rangeInfo);

        void CreateAccelerationStructure(VkAccelerationStructureTypeKHR asType,
            AccelerationStructure& accelStruct,
            VkAccelerationStructureGeometryKHR& asGeometry,
            VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,
            VkBuildAccelerationStructureFlagsKHR flags,
            AccelerationStructure const* srcAccel = nullptr,
            VkBuildAccelerationStructureModeKHR buildMode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR);

        void CreateBLAS();
        void CreateTLAS();
        void UpdateTopLevelASImmediate(const std::vector<VkAccelerationStructureInstanceKHR>& instances);
    };
}
