#pragma once

namespace Dog 
{
	class Window {
	public:
		Window(int w, int h, std::wstring_view name);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		bool ShouldClose() { return glfwWindowShouldClose(mWindow); }
		VkExtent2D GetExtent() { return { static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight) }; }
		bool WasWindowResized() { return mFramebufferResized; }
		void ResetWindowResizedFlag() { mFramebufferResized = false; }
		GLFWwindow* GetGLFWwindow() const { return mWindow; }
		void SetWindowTitle(const char* title) { glfwSetWindowTitle(mWindow, title); }

		void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

		const int& GetWidth() const { return mWidth; }
		const int& GetHeight() const { return mHeight; }

	private:
		static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
		void InitWindow();

		int mWidth;
		int mHeight;
		bool mFramebufferResized = false;

		std::wstring mWindowName;
		GLFWwindow* mWindow;
	};

} // namespace Dog
