#include <PCH/pch.h>
#include "SceneWindow.h"

#include "Graphics/OpenGL/GLFrameBuffer.h"

#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "Engine.h"

namespace Dog
{
    namespace EditorWindows
    {
        void RenderSceneWindow(RenderingResource* rr, EditorResource* er)
        {
            // Create a window and display the scene texture
            void* sceneTexturePtr;
            if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
            {
                sceneTexturePtr = reinterpret_cast<void*>(rr->sceneTextureDescriptorSet);
            }
            else
            {
                sceneTexturePtr = (void*)(uintptr_t)(rr->sceneFrameBuffer->GetColorAttachmentID(0));
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("Viewport");

            float flipY = static_cast<float>(Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL);
            ImVec2 uv0 = { 0.0f, flipY * 1.0f + (1.0f - flipY) * 0.0f };
            ImVec2 uv1 = { 1.0f, flipY * 0.0f + (1.0f - flipY) * 1.0f };

            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            ImGui::Image(sceneTexturePtr, viewportSize, uv0, uv1);
            ImGui::End();
            ImGui::PopStyleVar();

            if (er->sceneWindowWidth != viewportSize.x ||
                er->sceneWindowHeight != viewportSize.y)
            {
                PUBLISH_EVENT(Event::SceneResize, (int)viewportSize.x, (int)viewportSize.y);
            }
            er->sceneWindowWidth = viewportSize.x;
            er->sceneWindowHeight = viewportSize.y;
        }
    }
}
