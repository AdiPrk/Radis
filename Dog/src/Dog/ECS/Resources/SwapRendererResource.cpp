#include <PCH/pch.h>
#include "SwapRendererResource.h"
#include "Engine.h"

namespace Dog
{
    void SwapRendererResource::RequestVulkan()
    {
        swapRequested = Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan;
    }

    void SwapRendererResource::RequestOpenGL()
    {
        swapRequested = Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL;
    }
}
