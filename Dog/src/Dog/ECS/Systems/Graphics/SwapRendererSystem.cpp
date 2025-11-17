#include <PCH/pch.h>
#include "SwapRendererSystem.h"

#include "ECS/Resources/renderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/SwapRendererResource.h"
#include "../InputSystem.h"
#include "Engine.h"

#include "Graphics/OpenGL/GLShader.h"
#include "Graphics/Vulkan/Core/Device.h"

#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/TextureLibrary.h"

namespace Dog
{
    void SwapRendererSystem::Init()
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

    void SwapRendererSystem::FrameStart()
    {
        auto sr = ecs->GetResource<SwapRendererResource>();

        if (sr->SwapRequested())
        {
            SwapBackend();
            sr->swapRequested = false;
        }
    }

    void SwapRendererSystem::SwapBackend()
    {
        bool hasEditor = Engine::GetEditorEnabled();

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
            EditorResource* er = nullptr;
            if (hasEditor) {
                er = ecs->GetResource<EditorResource>();
                er->Cleanup(rr->device.get());
            }
            rr->Cleanup();
            wr->Cleanup();
            Engine::SetGraphicsAPI(GraphicsAPI::Vulkan);
            wr->Create();
            rr->Create(wr->window.get());
            if (hasEditor) {
                er->Create(rr->device.get(), rr->swapChain.get(), wr->window->GetGLFWwindow(), wr->window->GetDPIScale());
                er->CreateSceneTextures(rr);
            }

            rr->modelLibrary->RecreateAllBuffers(rr->device.get());
            //rr->textureLibrary->RecreateAllBuffers(rr->device.get());
            InputSystem::ResetWindow(wr->window->GetGLFWwindow());
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            auto wr = ecs->GetResource<WindowResource>();
            EditorResource* er = nullptr;

            if (rr->device->SupportsVulkan()) 
            {
                vkDeviceWaitIdle(rr->device->GetDevice());
            }

            if (hasEditor) {
                er = ecs->GetResource<EditorResource>();
                er->Cleanup(rr->device.get());
            }
            rr->Cleanup();
            wr->Cleanup();
            Engine::SetGraphicsAPI(GraphicsAPI::OpenGL);
            wr->Create();
            rr->Create(wr->window.get());
            if (hasEditor) {
                er->Create(wr->window->GetGLFWwindow(), wr->window->GetDPIScale());
            }

            rr->modelLibrary->RecreateAllBuffers(rr->device.get());
            //rr->textureLibrary->RecreateAllBuffers(rr->device.get());
            InputSystem::ResetWindow(wr->window->GetGLFWwindow());
        }
    }
}
