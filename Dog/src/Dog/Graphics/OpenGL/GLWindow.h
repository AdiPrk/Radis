#pragma once

#include "../IWindow.h"

namespace Dog 
{
	class GLWindow : public IWindow {
	public:
		GLWindow(int w, int h, std::wstring_view name);
		~GLWindow();

		GLWindow(const GLWindow&) = delete;
		GLWindow& operator=(const GLWindow&) = delete;

	private:
		void InitializeContext() override;
        void InitializeDebugCallbacks();
		void SwapBuffers() override;
	};

} // namespace Dog
