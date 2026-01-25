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

namespace Radis
{
    void SwapRendererSystem::Init()
    {
        auto sr = ecs->GetResource<SwapRendererResource>();

        bool canVulkan = Engine::GetVulkanSupported();
        bool isVulkan = (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan);
        bool requiresSwap = false;
        if (!canVulkan && isVulkan)
        {
            requiresSwap = true;
        }

        if (requiresSwap)
        {
            sr->SwapBackend(ecs);
        }
    }

    void SwapRendererSystem::FrameStart()
    {
        auto sr = ecs->GetResource<SwapRendererResource>();

        if (sr->SwapRequested())
        {
            sr->SwapBackend(ecs);
            sr->swapRequested = false;
        }
    }

    void SwapRendererSystem::FrameEnd()
    {
        auto er = ecs->GetResource<EditorResource>();
        if (!er) return;

        if (!er->entitiesToDelete.empty())
        {
            // Remove entities to delete from selected entities list
            for (const auto& entityToDelete : er->entitiesToDelete)
            {
                er->selectedEntities.erase(
                    std::remove_if(
                        er->selectedEntities.begin(),
                        er->selectedEntities.end(),
                        [&](const Entity& selectedEntity)
                        {
                            return selectedEntity == entityToDelete;
                        }),
                    er->selectedEntities.end());
                
                ecs->RemoveEntity(entityToDelete);
            }

            er->entitiesToDelete.clear();
        }
    }
}
