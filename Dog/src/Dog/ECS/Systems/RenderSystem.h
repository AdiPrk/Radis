#pragma once

#include "ISystem.h"

namespace Dog
{
    class Pipeline;

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

        void RenderScene(VkCommandBuffer cmd);

        void RenderSkeleton();
        void RecursiveNodeDraw(const glm::mat4& parentTr, const aiNode* node);

    private:
        std::unique_ptr<Pipeline> mPipeline;
        std::unique_ptr<Pipeline> mWireframePipeline;

        std::vector<VkBuffer> mIndirectBuffers;
        std::vector<VmaAllocation> mIndirectBufferAllocations;
    };
}

