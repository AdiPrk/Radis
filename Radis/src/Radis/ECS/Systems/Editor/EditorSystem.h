/*****************************************************************//**
 * \file   EditorSystem.h
 * \brief  System for managing the editor
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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
        void RenderDebugWindow();

        void RenderMainMenuBar();

        bool mMouseDown = false;
        bool mLockMouse = false;
    };
}