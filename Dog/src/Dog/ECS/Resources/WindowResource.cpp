#include <PCH/pch.h>
#include "WindowResource.h"
#include "Graphics/Vulkan/VulkanWindow.h"
#include "Graphics/OpenGL/GLWindow.h"
#include "Engine.h"

namespace Dog
{
    WindowResource::WindowResource(int w, int h, std::wstring_view name)
    {
        Create(w, h, name);
    }

    void WindowResource::Create(int w, int h, std::wstring_view name)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            window = std::make_unique<VulkanWindow>(w, h, name);
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            window = std::make_unique<GLWindow>(w, h, name);
        }
        else
        {
            DOG_CRITICAL("Unsupported Graphics API!");
        }
    }

    void WindowResource::Cleanup()
    {
        window.reset();
    }
}

