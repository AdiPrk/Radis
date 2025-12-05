#include <PCH/pch.h>
#include "WindowResource.h"
#include "Graphics/Vulkan/VulkanWindow.h"
#include "Graphics/OpenGL/GLWindow.h"
#include "Engine.h"

namespace Dog
{
    WindowResource::WindowResource(int w, int h, std::wstring_view name)
    {
        if (!glfwInit())
        {
            DOG_CRITICAL("Failed to initialize GLFW");
        }

        Create(w, h, name);
    }

    void WindowResource::Create()
    {
        Create(IWindow::GetWidth(), IWindow::GetHeight(), L"ÉèÉìÉèÉì");
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
        if (window)
        {
            // Store window location for the next window
            GLFWwindow* glfwWindow = window->GetGLFWwindow();
            glfwGetWindowPos(glfwWindow, &IWindow::xPos, &IWindow::yPos);
        }

        window.reset();
    }

    void WindowResource::Shutdown()
    {
        Cleanup();
        glfwTerminate();
    }
}

