#pragma once

#include "ISystem.h"

namespace Dog
{
    class Pipeline;

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

    private:
        std::unique_ptr<Pipeline> mPipeline;
        std::unique_ptr<Pipeline> mWireframePipeline;

        std::vector<VkBuffer> mIndirectBuffers;
        std::vector<VmaAllocation> mIndirectBufferAllocations;
    };
}

