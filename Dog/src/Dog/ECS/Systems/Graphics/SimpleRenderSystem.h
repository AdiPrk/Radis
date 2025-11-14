#pragma once

#include "../ISystem.h"
#include "Graphics/Vulkan/Core/AccelerationStructures.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

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
        void RaytraceScene(VkCommandBuffer cmd);

    private:
        uint32_t mNumObjectsRendered = 0;
        uint32_t mConstStartingObjectCount = 0;
        std::vector<MeshDataUniform> mRTMeshData{};
        std::vector<uint32_t> mRTMeshIndices{};
    };
}

