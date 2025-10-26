#pragma once

#include "../IWindow.h"

namespace Dog 
{
	class OpenGLWindow : public IWindow {
	public:
		OpenGLWindow(int w, int h, std::wstring_view name);
		~OpenGLWindow();

		OpenGLWindow(const OpenGLWindow&) = delete;
		OpenGLWindow& operator=(const OpenGLWindow&) = delete;

	private:
		void InitializeContext() override;
        void InitializeDebugCallbacks();
		void SwapBuffers() override;
	};

} // namespace Dog
