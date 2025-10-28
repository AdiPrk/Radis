#include <PCH/pch.h>
#include "IWindow.h"
#include "Utils/Utils.h"

namespace Dog
{
    int IWindow::mWidth = 800;
    int IWindow::mHeight = 600;
    int IWindow::xPos = -1;
    int IWindow::yPos = -1;

    IWindow::IWindow(int w, int h, std::wstring_view name, GraphicsAPI api)
        : mTitle{ name }
        , mAPI{ api }
    {
        mWidth = w;
        mHeight = h;

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

    void IWindow::SetTitle(const std::wstring& title)
    {
        mTitle = title;
        std::string title_utf8 = WStringToUTF8(mTitle);

        glfwSetWindowTitle(mWindow, title_utf8.c_str());
    }

    void IWindow::InitGLFWWindow()
    {
        static bool ft = true;
        if (ft)
        {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            GLFWwindow* dummy = glfwCreateWindow(1, 1, "", nullptr, nullptr);
            glfwDestroyWindow(dummy);
            ft = false;
        }

        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        std::string title_utf8 = WStringToUTF8(mTitle);
        mWindow = glfwCreateWindow(mWidth, mHeight, title_utf8.c_str(), nullptr, nullptr);
        if (!mWindow)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(mWindow, this);
        glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);

        if (xPos != -1 && yPos != -1)
        {
            glfwSetWindowPos(mWindow, xPos, yPos);
        }

        glfwSetWindowSizeLimits(mWindow, 100, 50, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }
    
    void IWindow::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto win = reinterpret_cast<IWindow*>(glfwGetWindowUserPointer(window));
        win->mFramebufferResized = true;
        
        IWindow::mWidth = width;
        IWindow::mHeight = height;
    }

    const char** IWindow::GetRequiredVulkanExtensions(uint32_t* count) const
    {
        if (count) *count = 0;
        return nullptr;
    }
}
