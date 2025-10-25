#include <PCH/pch.h>
#include "IWindow.h"
#include "Utils/Utils.h"

#include <GLFW/glfw3.h>

namespace Dog
{
    IWindow::IWindow(int w, int h, std::wstring_view name, GraphicsAPI api)
        : mWidth{ w }
        , mHeight{ h }
        , mTitle{ name }
        , mAPI{ api }
    {
        if (!glfwInit()) 
        {
            DOG_CRITICAL("Failed to initialize GLFW");
        }
    }

    IWindow::~IWindow()
    {
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }

    void IWindow::PollEvents()
    {
        glfwPollEvents();
    }

    void IWindow::WaitEvents()
    {
        glfwWaitEvents();
    }

    bool IWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(mWindow);
    }

    glm::uvec2 IWindow::GetExtent() const
    {
        return glm::uvec2(static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight));
    }

    void IWindow::SetTitle(const std::string& title)
    {
        std::string title_utf8 = WStringToUTF8(mTitle);
        glfwSetWindowTitle(mWindow, title_utf8.c_str());
    }

    void IWindow::InitGLFWWindow()
    {
        std::string title_utf8 = WStringToUTF8(mTitle);
        mWindow = glfwCreateWindow(mWidth, mHeight, title_utf8.c_str(), nullptr, nullptr);
        if (!mWindow) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(mWindow, this);
        glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);
    }
    
    void IWindow::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto win = reinterpret_cast<IWindow*>(glfwGetWindowUserPointer(window));
        win->mFramebufferResized = true;
        win->mWidth = width;
        win->mHeight = height;
    }

    const char** IWindow::GetRequiredVulkanExtensions(uint32_t* count) const
    {
        if (count) *count = 0;
        return nullptr;
    }
}
