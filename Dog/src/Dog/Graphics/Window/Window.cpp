#include <PCH/pch.h>
#include "Window.h"

std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return {};

    // First, get the required buffer size
    int size_needed = WideCharToMultiByte(
        CP_UTF8,             // convert to UTF-8
        0,                   // no special flags
        wstr.data(),         // source string
        static_cast<int>(wstr.size()), // length of source
        nullptr,             // no output yet
        0,                   // calculate required size
        nullptr, nullptr     // default handling for invalid chars
    );

    std::string result(size_needed, 0);

    // Do the actual conversion
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.data(),
        static_cast<int>(wstr.size()),
        result.data(),
        size_needed,
        nullptr, nullptr
    );

    return result;
}

namespace Dog {

    Window::Window(int w, int h, std::wstring_view name) : mWidth{ w }, mHeight{ h }, mWindowName{ name } {
        InitWindow();
    }

    Window::~Window() {
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }

    void Window::InitWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        std::string title_utf8 = WStringToUTF8(mWindowName);
        mWindow = glfwCreateWindow(mWidth, mHeight, title_utf8.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(mWindow, this);
        glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);
    }

    void Window::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
        if (glfwCreateWindowSurface(instance, mWindow, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to craete window surface");
        }
    }

    void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        win->mFramebufferResized = true;
        win->mWidth = width;
        win->mHeight = height;
    }

} // namespace Dog