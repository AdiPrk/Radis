/*****************************************************************//**
 * \file   RenderSystem.h
 * \brief  Handles rendering the scene
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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
        // ----------------------------
        // Vulkan
        // ----------------------------
        void RenderSceneVK(VkCommandBuffer cmd);

        void RenderShadowMomentsVK(VkCommandBuffer cmd);
        void RenderShadowBlurHVK(VkCommandBuffer cmd);
        void RenderShadowBlurVVK(VkCommandBuffer cmd);
        void RenderSceneDeferredGeometryVK(VkCommandBuffer cmd);
        void RenderSceneDeferredLightingVK(VkCommandBuffer cmd);
        void RenderLightVolumesVK(VkCommandBuffer cmd);
        void RenderToneMapVK(VkCommandBuffer cmd);

        void RaytraceSceneVK(VkCommandBuffer cmd);

        // -----------------------------
        // OpenGl
        // -----------------------------
        void RenderSceneGL();

        // Submit instanced draw calls
        void ExecuteInstancedDrawCalls(VkCommandBuffer cmd);
        
        // Utilities
        void UpdateShadowParamsUBO(struct RenderingResource& rr, int frameIndex);

        void SetViewportAndScissor(VkCommandBuffer cmd, const VkExtent2D& extent);
        CameraUniforms CollectCameraData(float aspectRatio);
        void CollectLightData();
        void BuildInstanceData();
        float GetAspectRatio();

        std::vector<MeshDataUniform> mRTMeshData{};
        std::vector<uint32_t> mRTMeshIndices{};

        std::vector<InstanceUniforms> mInstanceData{};
        std::vector<InstancedDrawCall> mDrawCalls{};
        std::vector<LightUniform> mLightData{};
        std::vector<uint8_t> mLightBuffer{};

        std::unordered_map<uint32_t, uint32_t> mMeshInstanceCounts{};

        ShadowCameraUniform mShadowCamData;

        // Light counts for instanced rendering
        uint32_t mDirectionalLightCount = 0;
        uint32_t mLocalLightCount = 0;
    };
}

