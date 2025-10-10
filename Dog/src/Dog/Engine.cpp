#include <PCH/pch.h>
#include "Engine.h"

#include "ECS/Systems/InputSystem.h"
#include "ECS/Systems/AnimationSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/Systems/EditorSystem.h"
#include "ECS/Systems/PresentSystem.h"
#include "ECS/Systems/CameraSystem.h"
#include "ECS/Systems/RayRenderSystem.h"

#include "ECS/Resources/InputResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/SerializationResource.h"
#include "ECS/Resources/AnimationResource.h"
#include "ECS/Resources/DebugDrawResource.h"

#include "Graphics/Window/FrameRate.h"
#include "Graphics/Vulkan/Core/Device.h"

namespace Dog 
{
    Engine::Engine(const EngineSpec& specs)
        : mSpecs(specs)
        , mEcs()
    {
        Logger::Init();

        // Systems -------------------------
        mEcs.AddSystem<InputSystem>();
        mEcs.AddSystem<PresentSystem>();
        mEcs.AddSystem<AnimationSystem>();
        // mEcs.AddSystem<RenderSystem>();
        mEcs.AddSystem<RayRenderSystem>();
        mEcs.AddSystem<EditorSystem>();
        mEcs.AddSystem<CameraSystem>();
        // ---------------------------------

        // Resources -----------------------
        mEcs.AddResource<WindowResource>(specs.width, specs.height, specs.name);

        auto wr = mEcs.GetResource<WindowResource>();
        mEcs.AddResource<InputResource>(wr->window->GetGLFWwindow());
        mEcs.AddResource<RenderingResource>(*wr->window);

        auto rr = mEcs.GetResource<RenderingResource>();
        mEcs.AddResource<EditorResource>(*rr->device, *rr->swapChain, *wr->window);
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

        vkDeviceWaitIdle(mEcs.GetResource<RenderingResource>()->device->GetDevice());
        mEcs.Exit();

        return EXIT_SUCCESS;
    }

} // namespace Dog