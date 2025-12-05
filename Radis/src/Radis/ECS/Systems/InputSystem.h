#pragma once

#include "ISystem.h"

#include "Utils/InputMap.h"

struct GLFWwindow;
struct GLFWcursor;

namespace Radis
{
    class InputSystem : public ISystem
    {
    public:
        InputSystem() : ISystem("InputSystem") {};
        ~InputSystem() {}

        void Init();
		void FrameStart();
        void Update(float dt);

		static bool isKeyDown(const Key& key);
		static bool isKeyTriggered(const Key& key);
		static bool isKeyReleased(const Key& key);
		static bool isMouseDown(const Mouse& button);

		// Init/exit
		static void ResetWindow(GLFWwindow* win);

		// Mouse delta
		static float GetMouseDeltaX();
		static float GetMouseDeltaY();

		// screen mouse positions
		static glm::vec2 getMouseScreenPosition() { return { mouseScreenX, mouseScreenY }; }
		static float getMouseScreenX() { return mouseScreenX; }
		static float getMouseScreenY() { return mouseScreenY; }
		static float getMouseScreenXDelta() { return mouseScreenX - lastMouseScreenX; }
		static float getMouseScreenYDelta() { return mouseScreenY - lastMouseScreenY; }

		// world mouse positions
		static glm::vec2 getMouseWorldPosition() { return { mouseWorldX, mouseWorldY }; }
		static float getMouseWorldX() { return mouseWorldX; }
		static float getMouseWorldY() { return mouseWorldY; }
		static float getMouseWorldXDelta() { return mouseWorldX - lastMouseWorldX; }
		static float getMouseWorldYDelta() { return mouseWorldY - lastMouseWorldY; }

		// scene mouse positions
		static glm::vec2 getMouseScenePosition() { return { mouseSceneX, mouseSceneY }; }
		static float getMouseSceneX() { return mouseSceneX; }
		static float getMouseSceneY() { return mouseSceneY; }

		// mouse scroll
		static float getMouseScrollX() { return scrollX; }
		static float getMouseScrollY() { return scrollY; }
		static float getMouseScrollDeltaX() { return scrollX - lastScrollX; }
		static float getMouseScrollDeltaY() { return scrollY - lastScrollY; }

		static void SetKeyInputLocked(bool locked);
		static void SetMouseInputLocked(bool locked);
        static bool IsKeyInputLocked() { return keyInputLocked; }
        static bool IsMouseInputLocked() { return mouseInputLocked; }
		
		static void WrapMouseOverScreen();
		static void DisableCursor();
		static void EnableCursor();

		struct KeyStates
		{
			bool keyDown = false;
			bool prevKeyDown = false;
		};

		struct MouseStates
		{
			bool mouseDown = false;
			bool mouseTriggered = false;
			bool mouseReleased = false;
			bool prevTriggered = false;
			bool doTriggered = false;
		};

		static KeyStates keyStates[static_cast<int>(Key::LAST)];
		static MouseStates mouseStates[static_cast<int>(Mouse::LAST)];

	private:

		static GLFWwindow* pwindow;

		static GLFWcursor* standardCursor;

		static float scrollX;
		static float scrollY;
		static float lastScrollX;
		static float lastScrollY;
		static float lastMouseScreenX;
		static float lastMouseScreenY;
		static float mouseScreenX;
		static float mouseScreenY;
		static float lastMouseWorldX;
		static float lastMouseWorldY;
		static float lastMouseSceneX;
		static float lastMouseSceneY;
		static float mouseWorldX;
		static float mouseWorldY;
		static float mouseSceneX;
		static float mouseSceneY;

		static bool keyInputLocked;
		static bool mouseInputLocked;
		static bool charInputLocked;

		static void keyPressCallback(GLFWwindow* windowPointer, int key, int scanCode, int action, int mod);
		static void mouseButtonCallback(GLFWwindow* windowPointer, int mouseButton, int action, int mod);
		static void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
		static void charCallback(GLFWwindow* window, unsigned int codepoint);
    };
}