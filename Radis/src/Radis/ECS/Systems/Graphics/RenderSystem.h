#pragma once

#include "../ISystem.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

namespace Radis
{
    // Represents a batched draw call for instanced rendering
    struct InstancedDrawCall
    {
        uint32_t meshID;
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
        uint32_t instanceCount;
        uint32_t firstInstance;
    };

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


    private:
        void RenderSceneVK(VkCommandBuffer cmd);

        void RenderSceneDeferredGeometryVK(VkCommandBuffer cmd);
        void RenderSceneDeferredLightingVK(VkCommandBuffer cmd);

        void RaytraceSceneVK(VkCommandBuffer cmd);

        void RenderSceneGL();

        void ExecuteInstancedDrawCalls(VkCommandBuffer cmd);
        
        float GetAspectRatio();

        std::vector<MeshDataUniform> mRTMeshData{};
        std::vector<uint32_t> mRTMeshIndices{};

        std::vector<InstanceUniforms> mInstanceData{};
        std::vector<InstancedDrawCall> mDrawCalls{};
        std::vector<LightUniform> mLightData{};
        std::vector<uint8_t> mLightBuffer{};

        std::unordered_map<uint32_t, uint32_t> mMeshInstanceCounts{};
    };
}

