#pragma once

namespace Dog
{
	class IWindow {
	public:
		IWindow(int w, int h, std::wstring_view name, GraphicsAPI api);
		virtual ~IWindow();

		IWindow(const IWindow&) = delete;
		IWindow& operator=(const IWindow&) = delete;

		// Core window operations
		virtual void PollEvents();
		virtual void WaitEvents();
		virtual void SwapBuffers() = 0;  // Only relevant for GL/DX; Vulkan may no-op
		bool ShouldClose() const;

		// Window state
		glm::uvec2 GetExtent() const;
		static int GetWidth() { return mWidth; }
		static int GetHeight() { return mHeight; }

		bool WasResized() const { return mFramebufferResized; }
		void ResetResizeFlag() { mFramebufferResized = false; }

		void SetTitle(const std::wstring& title);

        // Get glfw window handle
		GLFWwindow* GetGLFWwindow() const { return mWindow; }

		// Backend-specific setup
		virtual void InitializeContext() = 0;

		// For vulkan
		virtual void CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) {};
		virtual const char** GetRequiredVulkanExtensions(uint32_t* count) const;

        // Get the graphics API used by this window
		GraphicsAPI GetAPI() const { return mAPI; }

	protected:
		void InitGLFWWindow();
		static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

	protected:
		friend struct WindowResource;
        static int mWidth;
        static int mHeight;
		static int xPos;
        static int yPos;

        bool mFramebufferResized = false;

        std::wstring mTitle;
        GLFWwindow* mWindow = nullptr;
        GraphicsAPI mAPI = GraphicsAPI::None;
    };

} // namespace Dog
