#include <PCH/pch.h>
#include "SwapRendererResource.h"
#include "Engine.h"

#include "renderingResource.h"
#include "EditorResource.h"
#include "WindowResource.h"
#include "ECS/Systems/InputSystem.h"
#include "Engine.h"

#include "Graphics/OpenGL/GLShader.h"
#include "Graphics/Vulkan/Core/Device.h"

#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/TextureLibrary.h"

namespace Dog
{
    void SwapRendererResource::RequestVulkan()
    {
        swapRequested = Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan;
    }

    void SwapRendererResource::RequestOpenGL()
    {
        swapRequested = Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL;
    }

    void SwapRendererResource::SwapBackend(ECS* ecs, bool isAtInitializaton)
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

            if (rr->device && rr->device->SupportsVulkan())
            {
                vkDeviceWaitIdle(rr->device->GetDevice());
            }

            if (hasEditor && !isAtInitializaton) {
                er = ecs->GetResource<EditorResource>();
                er->Cleanup(rr->device.get());
            }
            rr->Cleanup();
            wr->Cleanup();
            Engine::SetGraphicsAPI(GraphicsAPI::OpenGL);
            wr->Create();
            rr->Create(wr->window.get());
            if (hasEditor && !isAtInitializaton) {
                er->Create(wr->window->GetGLFWwindow(), wr->window->GetDPIScale());
            }

            if (!isAtInitializaton) 
            {
                rr->modelLibrary->RecreateAllBuffers(rr->device.get());
            }
            //rr->textureLibrary->RecreateAllBuffers(rr->device.get());
            InputSystem::ResetWindow(wr->window->GetGLFWwindow());
        }
    }
}
