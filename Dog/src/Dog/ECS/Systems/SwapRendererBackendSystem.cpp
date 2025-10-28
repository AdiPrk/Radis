#include <PCH/pch.h>
#include "SwapRendererBackendSystem.h"

#include "../Resources/renderingResource.h"
#include "../Resources/EditorResource.h"
#include "../Resources/WindowResource.h"
#include "InputSystem.h"
#include "Engine.h"

#include "Graphics/OpenGL/GLShader.h"
#include "Graphics/Vulkan/Core/Device.h"

namespace Dog
{
    void SwapRendererBackendSystem::FrameStart()
    {
        if (InputSystem::isKeyTriggered(Key::K))
        {
            DOG_INFO("Key K pressed - Swapping Renderer Backend is disabled in this build.");
        }

        if (InputSystem::isKeyTriggered(Key::K))
        {
            if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
            {
                auto rr = ecs->GetResource<RenderingResource>();
                auto wr = ecs->GetResource<WindowResource>();
                auto er = ecs->GetResource<EditorResource>();
                er->Cleanup(rr->device.get());
                rr->Cleanup();
                wr->Cleanup();
                Engine::SetGraphicsAPI(GraphicsAPI::Vulkan);
                wr->Create(1280, 720, L"ƒƒ“ƒƒ“ (VK)");
                rr->Create(wr->window.get());
                er->Create(*rr->device, *rr->swapChain, wr->window->GetGLFWwindow());
                er->CreateSceneTextures(rr);
                InputSystem::ResetWindow(wr->window->GetGLFWwindow());
            }
            else if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
            {
                auto rr = ecs->GetResource<RenderingResource>();
                auto wr = ecs->GetResource<WindowResource>();
                auto er = ecs->GetResource<EditorResource>();

                vkDeviceWaitIdle(rr->device->GetDevice());

                er->Cleanup(rr->device.get());
                rr->Cleanup();
                wr->Cleanup();
                Engine::SetGraphicsAPI(GraphicsAPI::OpenGL);
                wr->Create(1280, 720, L"ƒƒ“ƒƒ“ (GL)");
                rr->Create(wr->window.get());
                er->Create(wr->window->GetGLFWwindow());
                InputSystem::ResetWindow(wr->window->GetGLFWwindow());
            }
        }
    }
}
