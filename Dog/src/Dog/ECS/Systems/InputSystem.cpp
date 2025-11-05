#include <PCH/pch.h>
#include "InputSystem.h"

#include "ECS/ECS.h"
#include "ECS/Resources/InputResource.h"
#include "ECS/Resources/WindowResource.h"
#include "Graphics/IWindow.h"

#include "GLFW/glfw3.h"

#define DO_INPUT_LOGGING 0

namespace Dog {

	InputSystem::KeyStates InputSystem::keyStates[static_cast<int>(Key::LAST)];
	InputSystem::MouseStates InputSystem::mouseStates[static_cast<int>(Mouse::LAST)];

	bool InputSystem::keyInputLocked = false;
	bool InputSystem::mouseInputLocked = false;
	bool InputSystem::charInputLocked = false;

	float InputSystem::scrollX = 0;
	float InputSystem::scrollY = 0;
	float InputSystem::lastScrollX = 0;
	float InputSystem::lastScrollY = 0;
	float InputSystem::mouseScreenX = 0;
	float InputSystem::mouseScreenY = 0;
	float InputSystem::lastMouseScreenX = 0;
	float InputSystem::lastMouseScreenY = 0;
	float InputSystem::mouseWorldX = 0;
	float InputSystem::mouseWorldY = 0;
	float InputSystem::lastMouseWorldX = 0;
	float InputSystem::lastMouseWorldY = 0;
	float InputSystem::lastMouseSceneX = 0;
	float InputSystem::lastMouseSceneY = 0;
	float InputSystem::mouseSceneX = 0;
	float InputSystem::mouseSceneY = 0;
	GLFWcursor* InputSystem::standardCursor = nullptr;
	GLFWwindow* InputSystem::pwindow = nullptr;

	void InputSystem::Init()
	{
		InputSystem::pwindow = ecs->GetResource<WindowResource>()->window->GetGLFWwindow();

		glfwSetKeyCallback(InputSystem::pwindow, keyPressCallback);
		glfwSetMouseButtonCallback(InputSystem::pwindow, mouseButtonCallback);
		glfwSetScrollCallback(InputSystem::pwindow, mouseScrollCallback);
		glfwSetCharCallback(InputSystem::pwindow, charCallback);

		InputSystem::standardCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		glfwSetCursor(InputSystem::pwindow, InputSystem::standardCursor);

		double xpos, ypos;
		glfwGetCursorPos(pwindow, &xpos, &ypos);
		InputSystem::mouseScreenX = static_cast<float>(xpos);
		InputSystem::mouseScreenY = static_cast<float>(ypos);
		InputSystem::lastMouseScreenX = InputSystem::mouseScreenX;
		InputSystem::lastMouseScreenY = InputSystem::mouseScreenY;
	}

	struct ViewportInfo {
		GLint x, y, width, height;

		ViewportInfo() : x(), y(), width(), height() {};
		ViewportInfo(GLint info[4]) : x(info[0]), y(info[1]), width(info[2]), height(info[3]) {};
	};

	void InputSystem::FrameStart()
	{
		// Update last scroll values
		InputSystem::lastScrollX = InputSystem::scrollX;
		InputSystem::lastScrollY = InputSystem::scrollY;

		// Update last mouse coordinates before polling for new ones
		InputSystem::lastMouseScreenX = InputSystem::mouseScreenX;
		InputSystem::lastMouseScreenY = InputSystem::mouseScreenY;
		InputSystem::lastMouseWorldX = InputSystem::mouseWorldX;
		InputSystem::lastMouseWorldY = InputSystem::mouseWorldY;
		InputSystem::lastMouseSceneX = InputSystem::mouseSceneX;
		InputSystem::lastMouseSceneY = InputSystem::mouseSceneY;

		// Get current mouse screen coordinates from GLFW
		double xpos, ypos;
		glfwGetCursorPos(pwindow, &xpos, &ypos);
		InputSystem::mouseScreenX = static_cast<float>(xpos);
		InputSystem::mouseScreenY = static_cast<float>(ypos);

		for (int i = 0; i < static_cast<int>(Key::LAST); i++)
		{
			InputSystem::keyStates[i].prevKeyDown = InputSystem::keyStates[i].keyDown;
		}

		glfwPollEvents();
	}

	void InputSystem::Update(float dt)
	{
	}

	bool InputSystem::isKeyDown(const Key& key)
	{
		return keyStates[static_cast<int>(key)].keyDown;
	}

	bool InputSystem::isKeyTriggered(const Key& key)
	{
		return keyStates[static_cast<int>(key)].keyDown && !keyStates[static_cast<int>(key)].prevKeyDown;
	}

	bool InputSystem::isKeyReleased(const Key& key)
	{
		return !keyStates[static_cast<int>(key)].keyDown && keyStates[static_cast<int>(key)].prevKeyDown;
	}

	bool InputSystem::isMouseDown(const Mouse& button)
	{
		return mouseStates[static_cast<int>(button)].mouseDown;
	}

	void InputSystem::ResetWindow(GLFWwindow* win)
	{
		InputSystem::pwindow = win;

		glfwSetKeyCallback(InputSystem::pwindow, keyPressCallback);
		glfwSetMouseButtonCallback(InputSystem::pwindow, mouseButtonCallback);
		glfwSetScrollCallback(InputSystem::pwindow, mouseScrollCallback);
		glfwSetCharCallback(InputSystem::pwindow, charCallback);

		InputSystem::standardCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		glfwSetCursor(InputSystem::pwindow, InputSystem::standardCursor);

		double xpos, ypos;
		glfwGetCursorPos(pwindow, &xpos, &ypos);
		InputSystem::mouseScreenX = static_cast<float>(xpos);
		InputSystem::mouseScreenY = static_cast<float>(ypos);
		InputSystem::lastMouseScreenX = InputSystem::mouseScreenX;
		InputSystem::lastMouseScreenY = InputSystem::mouseScreenY;

		// reset all states
		for (int i = 0; i < static_cast<int>(Key::LAST); i++)
		{
			InputSystem::keyStates[i].keyDown = false;
			InputSystem::keyStates[i].prevKeyDown = false;
        }

		for (int i = 0; i < static_cast<int>(Mouse::LAST); i++)
		{
			InputSystem::mouseStates[i].mouseDown = false;
			InputSystem::mouseStates[i].mouseReleased = false;
			InputSystem::mouseStates[i].mouseTriggered = false;
			InputSystem::mouseStates[i].prevTriggered = false;
            InputSystem::mouseStates[i].doTriggered = false;
		}
	}

	float InputSystem::GetMouseDeltaX()
	{
		return mouseScreenX - lastMouseScreenX;
	}

	float InputSystem::GetMouseDeltaY()
	{
		return mouseScreenY - lastMouseScreenY;
	}

	void InputSystem::SetKeyInputLocked(bool locked)
	{
		keyInputLocked = locked;
	}

	void InputSystem::SetMouseInputLocked(bool locked)
	{
		mouseInputLocked = locked;
	}

	void InputSystem::WrapMouseOverScreen()
	{
		int windowWidth, windowHeight;
		glfwGetWindowSize(pwindow, &windowWidth, &windowHeight);
		double xpos = mouseScreenX;
		double ypos = mouseScreenY;
		bool wrapped = false;
		if (mouseScreenX < 0)
		{
			xpos = static_cast<double>(windowWidth - 1);
			wrapped = true;
		}
		else if (mouseScreenX >= windowWidth)
		{
			xpos = 0.0;
			wrapped = true;
		}
		if (mouseScreenY < 0)
		{
			ypos = static_cast<double>(windowHeight - 1);
			wrapped = true;
		}
		else if (mouseScreenY >= windowHeight)
		{
			ypos = 0.0;
			wrapped = true;
		}
		if (wrapped)
		{
			glfwSetCursorPos(pwindow, xpos, ypos);
			mouseScreenX = static_cast<float>(xpos);
			mouseScreenY = static_cast<float>(ypos);
        }
	}

	void InputSystem::DisableCursor()
	{
		glfwSetInputMode(pwindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	void InputSystem::EnableCursor()
	{
		glfwSetInputMode(pwindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	void InputSystem::keyPressCallback(GLFWwindow* windowPointer, int key, int scanCode, int action, int mod)
	{
		ImGui_ImplGlfw_KeyCallback(windowPointer, key, scanCode, action, mod);

		// check if imgui is capturing the keyboard
		if (keyInputLocked) {
#if DO_INPUT_LOGGING
			DOG_INFO("Key {0} Ignored - ImGui is capturing it.", key);
#endif
			return;
		}

		if (key < 0 || key > static_cast<int>(Key::LAST))
			return;

		if (action == GLFW_PRESS)
		{
			keyStates[key].keyDown = true;
#if DO_INPUT_LOGGING
			DOG_INFO("Key Pressed: {0}", key);
#endif
		}
		else if (action == GLFW_RELEASE)
		{
			keyStates[key].keyDown = false;
#if DO_INPUT_LOGGING
			DOG_INFO("Key Released: {0}", key);
#endif
		}
	}

	void InputSystem::mouseButtonCallback(GLFWwindow* windowPointer, int mouseButton, int action, int mod)
	{
		ImGui_ImplGlfw_MouseButtonCallback(windowPointer, mouseButton, action, mod);

		// check if imgui is capturing the mouse
		if (mouseButton < 0 || mouseButton > static_cast<int>(Mouse::LAST))
			return;

		if (action == GLFW_PRESS && !mouseInputLocked)
		{
			mouseStates[mouseButton].mouseDown = true;
#if DO_INPUT_LOGGING
			DOG_INFO("Mouse Pressed: {0}", mouseButton);
#endif
		}
		else if (action == GLFW_RELEASE)
		{
			mouseStates[mouseButton].mouseDown = false;
#if DO_INPUT_LOGGING
			DOG_INFO("Mouse Released: {0}", mouseButton);
#endif
		}
	}

	void InputSystem::mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
	{
		ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

		if (mouseInputLocked) {
#if DO_INPUT_LOGGING
			DOG_INFO("Mouse Scroll Ignored - ImGui is capturing it.");
#endif
			return;
		}

		scrollX += float(xoffset);
		scrollY += float(yoffset);
#if DO_INPUT_LOGGING
		//DOG_INFO("Mouse Scrolled: {0}", degree);
#endif
	}

	void InputSystem::charCallback(GLFWwindow* window, unsigned int codepoint)
	{
		ImGui_ImplGlfw_CharCallback(window, codepoint);

		if (codepoint < 0x20 || codepoint > 0x7E) {
			// Ignore non-printable characters
			return;
		}

		if (charInputLocked) {
#if DO_INPUT_LOGGING
			DOG_INFO("Character Input Ignored - ImGui is capturing it.");
#endif
			return;
		}

#if DO_INPUT_LOGGING
		DOG_INFO("Character Input: {0}", codepoint);
#endif
	}

}
