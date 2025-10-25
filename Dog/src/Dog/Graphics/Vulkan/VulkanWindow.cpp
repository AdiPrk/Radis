#include <PCH/pch.h>
#include "VulkanWindow.h"
#include "Utils/Utils.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Dog {

    VulkanWindow::VulkanWindow(int w, int h, std::wstring_view name) 
        : IWindow(w, h, name, GraphicsAPI::Vulkan)
    {
        InitializeContext();
        InitGLFWWindow();
    }

    VulkanWindow::~VulkanWindow() 
    {
    }

    void VulkanWindow::InitializeContext()
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    }

    void VulkanWindow::CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) 
    {
        if (glfwCreateWindowSurface(instance, mWindow, nullptr, surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface");
        }
    }

    const char** VulkanWindow::GetRequiredVulkanExtensions(uint32_t* count) const
    {
        return glfwGetRequiredInstanceExtensions(count);
    }

} // namespace Dog