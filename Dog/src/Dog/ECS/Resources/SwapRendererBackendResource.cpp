#include <PCH/pch.h>
#include "SwapRendererBackendResource.h"
#include "Engine.h"

namespace Dog
{
    void SwapRendererBackendResource::RequestVulkan()
    {
        swapRequested = Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan;
    }

    void SwapRendererBackendResource::RequestOpenGL()
    {
        swapRequested = Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL;
    }
}
