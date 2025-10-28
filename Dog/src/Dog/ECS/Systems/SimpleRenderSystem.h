#pragma once

#include "ISystem.h"
#include "Graphics/Vulkan/Pipeline/Pipeline.h"


namespace Dog
{
    class Pipeline;

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
    };
}

