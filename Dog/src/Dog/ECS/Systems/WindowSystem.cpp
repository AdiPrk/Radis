#include <PCH/pch.h>
#include "WindowSystem.h"
#include "../Resources/WindowResource.h"

#include "ECS/ECS.h"
#include "Graphics/IWindow.h"

namespace Dog
{
    void WindowSystem::Update(float dt)
    {
        auto wr = ecs->GetResource<WindowResource>();
        if (!wr || !wr->window) return;
        wr->window->PollEvents();
    }

    void WindowSystem::FrameEnd()
    {
        auto wr = ecs->GetResource<WindowResource>();
        if (!wr || !wr->window) return;
        wr->window->SwapBuffers();
    }
}
