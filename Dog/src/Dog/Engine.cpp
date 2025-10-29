#include <PCH/pch.h>
#include "Engine.h"

#include "ECS/Systems/WindowSystem.h"
#include "ECS/Systems/InputSystem.h"
#include "ECS/Systems/AnimationSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/Systems/EditorSystem.h"
#include "ECS/Systems/PresentSystem.h"
#include "ECS/Systems/CameraSystem.h"
#include "ECS/Systems/RayRenderSystem.h"
#include "ECS/Systems/SimpleRenderSystem.h"
#include "ECS/Systems/SwapRendererBackendSystem.h"

#include "ECS/Resources/InputResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/SerializationResource.h"
#include "ECS/Resources/AnimationResource.h"
#include "ECS/Resources/DebugDrawResource.h"
#include "ECS/Resources/SwapRendererBackendResource.h"

#include "Utils/FrameRate.h"
#include "Graphics/Vulkan/Core/Device.h"

#include "Utils/Utils.h"

#include "Graphics/RHI/RHI.h"

namespace Dog 
{
    bool Engine::mDevBuild = false;
    GraphicsAPI Engine::mGraphicsAPI = GraphicsAPI::None;
    bool Engine::mVulkanSupported = true;

    Engine::Engine(const EngineSpec& specs, int argc, char* argv[])
        : mSpecs(specs)
        , mEcs()
    {
        ValidateStartingDirectory(argc, argv, &mDevBuild);
        SetGraphicsAPI(specs.graphicsAPI);

        Logger::Init();

        // Systems -------------------------
        mEcs.AddSystem<WindowSystem>();
        mEcs.AddSystem<InputSystem>();

        mEcs.AddSystem<SwapRendererBackendSystem>();
        mEcs.AddSystem<PresentSystem>();
        mEcs.AddSystem<SimpleRenderSystem>();
        mEcs.AddSystem<AnimationSystem>();

        mEcs.AddSystem<EditorSystem>();
        mEcs.AddSystem<CameraSystem>();
        // ---------------------------------

        // Resources -----------------------
        mEcs.AddResource<SwapRendererBackendResource>();
        mEcs.AddResource<WindowResource>(specs.width, specs.height, specs.name);

        auto wr = mEcs.GetResource<WindowResource>();
        mEcs.AddResource<InputResource>(wr->window->GetGLFWwindow());
        mEcs.AddResource<RenderingResource>(wr->window.get());

        if (specs.graphicsAPI == GraphicsAPI::Vulkan)
        {
            auto rr = mEcs.GetResource<RenderingResource>();
            mEcs.AddResource<EditorResource>(rr->device.get(), rr->swapChain.get(), wr->window->GetGLFWwindow());
        }
        else if (specs.graphicsAPI == GraphicsAPI::OpenGL)
        {
            mEcs.AddResource<EditorResource>(wr->window->GetGLFWwindow());
        }

        mEcs.AddResource<SerializationResource>();
        mEcs.AddResource<AnimationResource>();
        mEcs.AddResource<DebugDrawResource>();
        // ---------------------------------

        // Initialize ECS
        mEcs.Init();
    }

    Engine::~Engine() 
    {
    }

    // Main Loop!
    int Engine::Run(const std::string& sceneName) 
    {
        mEcs.GetResource<SerializationResource>()->Deserialize("Assets/scenes/" + sceneName + ".json");

        FrameRateController frameRateController(mSpecs.fps);
        while (!mEcs.GetResource<WindowResource>()->window->ShouldClose() && mRunning) 
        {
            float dt = frameRateController.WaitForNextFrame();

            mEcs.FrameStart();
            mEcs.Update(dt);
            mEcs.FrameEnd();
        }

        return Exit();
    }

    int Engine::Exit()
    {
        mRunning = false;

        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) 
        {
            vkDeviceWaitIdle(mEcs.GetResource<RenderingResource>()->device->GetDevice());
        }

        mEcs.Exit();

        return EXIT_SUCCESS;
    }

} // namespace Dog