#include <PCH/pch.h>
#include "SwapRendererBackendSystem.h"

#include "../Resources/renderingResource.h"
#include "../Resources/EditorResource.h"
#include "../Resources/WindowResource.h"
#include "../Resources/SwapRendererBackendResource.h"
#include "InputSystem.h"
#include "Engine.h"

#include "Graphics/OpenGL/GLShader.h"
#include "Graphics/Vulkan/Core/Device.h"

#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/TextureLibrary.h"

namespace Dog
{
    void SwapRendererBackendSystem::Init()
    {
        bool canVulkan = Engine::GetVulkanSupported();
        bool isVulkan = (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan);
        bool requiresSwap = false;
        if (!canVulkan && isVulkan)
        {
            requiresSwap = true;
        }

        if (requiresSwap)
        {
            SwapBackend();
        }
    }

    void SwapRendererBackendSystem::FrameStart()
    {
        auto sr = ecs->GetResource<SwapRendererBackendResource>();

        if (sr->SwapRequested())
        {
            SwapBackend();
            sr->swapRequested = false;
        }
    }

    void SwapRendererBackendSystem::SwapBackend()
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            bool vulkanSupported = Engine::GetVulkanSupported();
            if (!vulkanSupported)
            {
                DOG_WARN("Cannot switch to Vulkan; No suitable GPUs!");
                return;
            }

            auto rr = ecs->GetResource<RenderingResource>();
            auto wr = ecs->GetResource<WindowResource>();
            auto er = ecs->GetResource<EditorResource>();
            er->Cleanup(rr->device.get());
            rr->Cleanup();
            wr->Cleanup();
            Engine::SetGraphicsAPI(GraphicsAPI::Vulkan);
            wr->Create();
            rr->Create(wr->window.get());
            er->Create(rr->device.get(), rr->swapChain.get(), wr->window->GetGLFWwindow());
            er->CreateSceneTextures(rr);

            rr->modelLibrary->RecreateAllBuffers(rr->device.get());
            //rr->textureLibrary->RecreateAllBuffers(rr->device.get());
            InputSystem::ResetWindow(wr->window->GetGLFWwindow());
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            auto wr = ecs->GetResource<WindowResource>();
            auto er = ecs->GetResource<EditorResource>();

            if (rr->device->SupportsVulkan()) 
            {
                vkDeviceWaitIdle(rr->device->GetDevice());
            }

            er->Cleanup(rr->device.get());
            rr->Cleanup();
            wr->Cleanup();
            Engine::SetGraphicsAPI(GraphicsAPI::OpenGL);
            wr->Create();
            rr->Create(wr->window.get());
            er->Create(wr->window->GetGLFWwindow());

            rr->modelLibrary->RecreateAllBuffers(rr->device.get());
            //rr->textureLibrary->RecreateAllBuffers(rr->device.get());
            InputSystem::ResetWindow(wr->window->GetGLFWwindow());
        }
    }
}
