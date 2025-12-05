#pragma once

#include "Graphics/Vulkan/VulkanWindow.h"

#include "ECS/ECS.h"
#include "ECS/Resources/IResource.h"

namespace Radis 
{
	class Engine {
	public:
		Engine(const RadisLaunch::EngineSpec& specs, int argc, char* argv[]);
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
        static bool GetEditorEnabled() { return mEditorEnabled; }

	private:
		// Engine Specs
		RadisLaunch::EngineSpec mSpecs;

		// Is the engine running?
		bool mRunning = true;

		// Entity Component System for the engine
        ECS mEcs;

		// Configuration
        static bool mDevBuild;
		static GraphicsAPI mGraphicsAPI;
		static bool mVulkanSupported;
		static bool mEditorEnabled;
	};

} // namespace Radis
