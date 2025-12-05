#pragma once

#include "../ISystem.h"

namespace Radis
{
    class EditorSystem : public ISystem
    {
    public:
        EditorSystem() : ISystem("EditorSystem") {};
        ~EditorSystem() {}

        void Init();
        void FrameStart();
        void Update(float dt);
        void FrameEnd();
        void Exit();

    private:
        void RenderImGui(VkCommandBuffer cmd = VK_NULL_HANDLE);

        void RenderMainMenuBar();
        void RenderInspectorWindow();

        bool mMouseDown = false;
        bool mLockMouse = false;
    };
}