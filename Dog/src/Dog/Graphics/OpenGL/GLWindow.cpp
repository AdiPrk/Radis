#include <PCH/pch.h>
#include "GLWindow.h"
#include "Utils/Utils.h"

namespace Dog {

#define DEBUG_OPENGL_OUTPUT 1

#if DEBUG_OPENGL_OUTPUT
    void GLAPIENTRY OpenGLDebugCallback(GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam) {
        // Displaying the debug message
        if (type == GL_DEBUG_TYPE_OTHER) return;

        std::cerr << "OpenGL Debug Message (ID: " << id << "):\n";
        std::cerr << "Message: " << message << "\n";
        std::cerr << "Source: ";

        switch (source) {
        case GL_DEBUG_SOURCE_API: std::cerr << "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: std::cerr << "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cerr << "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY: std::cerr << "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION: std::cerr << "Application"; break;
        case GL_DEBUG_SOURCE_OTHER: std::cerr << "Other"; break;
        }

        std::cerr << "\nType: ";

        switch (type) {
        case GL_DEBUG_TYPE_ERROR: std::cerr << "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cerr << "Deprecated Behavior"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: std::cerr << "Undefined Behavior"; break;
        case GL_DEBUG_TYPE_PORTABILITY: std::cerr << "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE: std::cerr << "Performance"; break;
        case GL_DEBUG_TYPE_OTHER: std::cerr << "Other"; break;
        }

        std::cerr << "\nSeverity: ";

        switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH: std::cerr << "High"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: std::cerr << "Medium"; break;
        case GL_DEBUG_SEVERITY_LOW: std::cerr << "Low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: std::cerr << "Notification"; break;
        }

        std::cerr << "\n\n";

        if (type == GL_DEBUG_TYPE_ERROR) {
            __debugbreak();
        }
    }
#endif

    GLWindow::GLWindow(int w, int h, std::wstring_view name) 
        : IWindow(w, h, name, GraphicsAPI::OpenGL)
    {
        InitializeContext();
        InitGLFWWindow();
        
        glfwMakeContextCurrent(mWindow);

        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cout << "oh noes, glew didn't init properly!" << std::endl;
            return;
        }

        InitializeDebugCallbacks();
        std::cout << glGetString(GL_VERSION) << std::endl;

        // depth, msaa
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glfwSwapInterval(0); // No V-Sync
    }

    GLWindow::~GLWindow() 
    {
    }

    void GLWindow::InitializeContext()
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
    }

    void GLWindow::InitializeDebugCallbacks()
    {
#if DEBUG_OPENGL_OUTPUT
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OpenGLDebugCallback, nullptr);
        //glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif
    }

    void GLWindow::SwapBuffers()
    {
        glfwSwapBuffers(mWindow);
    }

} // namespace Dog