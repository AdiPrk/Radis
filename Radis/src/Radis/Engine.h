/*****************************************************************//**
 * \file   Engine.h
 * \brief  Core engine class - manages the application lifecycle
 * 
 * \author Aditya Prakash
 * \date   2026
 *********************************************************************/
#pragma once

#include "ECS/ECS.h"

namespace Radis
{
    /**
     * \brief Main engine class that orchestrates the entire application.
     * 
     * Manages the ECS, systems, resources, and the main game loop.
     * Non-copyable and non-movable to ensure single instance.
     */
    class Engine
    {
    public:
        Engine(const RadisLaunch::EngineSpec& specs, int argc, char* argv[]);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        /** Starts the main loop and runs until exit. */
        int Run(const std::string& sceneName);

        /** Signals the engine to shut down gracefully. */
        int Exit();

        // --- Static Configuration ---
        static void SetDevBuild(bool dev) { sDevBuild = dev; }
        static bool IsDevBuild() { return sDevBuild; }
        static GraphicsAPI GetGraphicsAPI() { return sGraphicsAPI; }
        static void SetGraphicsAPI(GraphicsAPI api) { sGraphicsAPI = api; }
        static void ForceVulkanUnsupportedSwap() { sVulkanSupported = false; }
        static bool GetVulkanSupported() { return sVulkanSupported; }
        static bool GetEditorEnabled() { return sEditorEnabled; }

    private:
        RadisLaunch::EngineSpec mSpecs;
        ECS mEcs;
        bool mRunning = true;

        static bool sDevBuild;
        static GraphicsAPI sGraphicsAPI;
        static bool sVulkanSupported;
        static bool sEditorEnabled;
    };

} // namespace Radis
