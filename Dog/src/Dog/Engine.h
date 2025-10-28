#pragma once

#include "Graphics/Vulkan/VulkanWindow.h"

#include "ECS/ECS.h"
#include "ECS/Resources/IResource.h"

namespace Dog 
{
	struct EngineSpec
	{
		std::wstring name = L"Dog Engine";             // The name of the window.
		unsigned width = 1280;                         // The width of the window.
		unsigned height = 720;                         // The height of the window.
		unsigned fps = 60;			                   // The target frames per second.
        std::string serverAddress = SERVER_IP;         // The address of the server. Defaults to online VPS server.
        uint16_t serverPort = 7777;                    // The port of the server.
        GraphicsAPI graphicsAPI = GraphicsAPI::Vulkan; // The graphics API to use.
	};

	class Engine {
	public:
		static constexpr int WIDTH = 1600;
		static constexpr int HEIGHT = 900;

		Engine(const EngineSpec& specs, int argc, char* argv[]);
		~Engine();

		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

		/*********************************************************************
		 * param:  sceneName: The name of the scene to run. (read from assets/scenes)
		 * 
		 * brief: Run the engine with the specified scene.
		 *********************************************************************/
		int Run(const std::string& sceneName);
		int Exit();

        // Configuration
		static void SetDevBuild(bool dev) { mDevBuild = dev; }
		static bool IsDevBuild() { return mDevBuild; }
        static GraphicsAPI GetGraphicsAPI() { return mGraphicsAPI; }
		static void SetGraphicsAPI(GraphicsAPI api) { mGraphicsAPI = api; }
		static void ForceVulkanUnsupportedSwap() { mVulkanSupported = false; }
        static bool GetVulkanSupported() { return mVulkanSupported; }

	private:
		// Engine Specs
		EngineSpec mSpecs;

		// Is the engine running?
		bool mRunning = true;

		// Entity Component System for the engine
        ECS mEcs;

		// Configuration
        static bool mDevBuild;
		static GraphicsAPI mGraphicsAPI;
		static bool mVulkanSupported;
	};

} // namespace Dog
