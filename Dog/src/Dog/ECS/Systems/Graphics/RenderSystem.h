#pragma once

#include "../ISystem.h"
#include "Graphics/Vulkan/Core/AccelerationStructures.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

namespace Dog
{
    class RenderSystem : public ISystem
    {
    public:
        RenderSystem();
        ~RenderSystem();

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

        void CreateAccelerationStructure(VkAccelerationStructureTypeKHR asType, 
            AccelerationStructure& accelStruct, 
            VkAccelerationStructureGeometryKHR& asGeometry, 
            VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo, 
            VkBuildAccelerationStructureFlagsKHR flags,
            AccelerationStructure const* srcAccel = nullptr,
            VkBuildAccelerationStructureModeKHR buildMode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR);

        void CreateBottomLevelAS();
        void CreateTopLevelAS();
        void UpdateBLASForModel(class Model* model);
        void BuildTLASFromInstances(const std::vector<VkAccelerationStructureInstanceKHR>& tlasInstances);

        void RaytraceScene(VkCommandBuffer cmd);

    private:
        uint32_t mNumObjectsRendered = 0;
        std::vector<MeshDataUniform> mRTMeshData{};
        std::vector<uint32_t> mRTMeshIndices{};

        std::vector<InstanceUniforms> mInstanceData{};
        std::vector<LightUniform> mLightData{};
    };
}

