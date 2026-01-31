/*****************************************************************//**
 * \file   GLWindow.h
 * \brief  Definition of the GLWindow class for OpenGL window management.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../IWindow.h"

namespace Radis 
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

} // namespace Radis
