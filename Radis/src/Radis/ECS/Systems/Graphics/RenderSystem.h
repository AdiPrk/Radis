#pragma once

#include "../ISystem.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

namespace Radis
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


    private:
        void RenderSceneVK(VkCommandBuffer cmd);
        void RaytraceSceneVK(VkCommandBuffer cmd);
        void RenderSceneGL();

        float GetAspectRatio();

        std::vector<MeshDataUniform> mRTMeshData{};
        std::vector<uint32_t> mRTMeshIndices{};

        std::vector<InstanceUniforms> mInstanceData{};
        std::vector<LightUniform> mLightData{};
        std::vector<uint8_t> mLightBuffer{};
    };
}

