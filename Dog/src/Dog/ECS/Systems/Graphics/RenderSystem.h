#pragma once

#include "../ISystem.h"
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

        void RaytraceScene(VkCommandBuffer cmd);

    private:
        uint32_t mNumObjectsRendered = 0;
        std::vector<MeshDataUniform> mRTMeshData{};
        std::vector<uint32_t> mRTMeshIndices{};

        std::vector<InstanceUniforms> mInstanceData{};
        std::vector<LightUniform> mLightData{};
    };
}

