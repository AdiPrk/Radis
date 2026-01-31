/*****************************************************************//**
 * \file   WindowSystem.cpp
 * \brief  Swaps the window buffer
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "WindowSystem.h"
#include "../Resources/WindowResource.h"

#include "ECS/ECS.h"
#include "Graphics/IWindow.h"

namespace Radis
{
    void WindowSystem::FrameEnd()
    {
        auto wr = ecs->GetResource<WindowResource>();
        if (!wr || !wr->window) return;
        wr->window->SwapBuffers();
    }
}
