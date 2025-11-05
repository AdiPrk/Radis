#include <PCH/pch.h>
#include "SceneWindow.h"

#include "Graphics/OpenGL/GLFrameBuffer.h"

#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Entities/Components.h"
#include "Engine.h"

namespace Dog
{
    namespace EditorWindows
    {
        void RenderSceneWindow(ECS* ecs)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            auto er = ecs->GetResource<EditorResource>();
            if (!rr || !er) return;

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

            // --- 2. Begin Window ---
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("Viewport");

            // --- 3. Get Viewport Rect ---
            // This is the CRITICAL part for ImGuizmo. We need the exact pixel coordinates
            // of the content area (where the image is drawn).
            ImVec2 contentRegionMin = ImGui::GetWindowContentRegionMin();
            ImVec2 contentRegionMax = ImGui::GetWindowContentRegionMax();
            ImVec2 windowPos = ImGui::GetWindowPos();

            // These are the screen-space coordinates ImGuizmo needs
            ImVec2 viewportTopLeft = ImVec2(windowPos.x + contentRegionMin.x, windowPos.y + contentRegionMin.y);
            ImVec2 viewportSize = ImVec2(contentRegionMax.x - contentRegionMin.x, contentRegionMax.y - contentRegionMin.y);

            // --- 4. Update EditorResource & Handle Resize ---
            // Update the resource *before* calling UpdateImGuizmo, so it uses the latest data.
            if (er->sceneWindowWidth != viewportSize.x ||
                er->sceneWindowHeight != viewportSize.y)
            {
                // Update dimensions in the resource
                er->sceneWindowWidth = viewportSize.x;
                er->sceneWindowHeight = viewportSize.y;

                // Publish resize event
                PUBLISH_EVENT(Event::SceneResize, (int)viewportSize.x, (int)viewportSize.y);
            }
            // Update position in the resource
            er->sceneWindowX = viewportTopLeft.x;
            er->sceneWindowY = viewportTopLeft.y;


            // --- 5. Calculate UVs (Unchanged) ---
            float flipY = static_cast<float>(Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL);
            ImVec2 uv0 = { 0.0f, flipY * 1.0f + (1.0f - flipY) * 0.0f };
            ImVec2 uv1 = { 1.0f, flipY * 0.0f + (1.0f - flipY) * 1.0f };

            // --- 6. (THE FIX) Draw Scene as Background ---
            // We use AddImage from the window's draw list. This does NOT create
            // a widget, so it won't capture mouse input.
            ImGui::GetWindowDrawList()->AddImage(
                sceneTexturePtr,
                viewportTopLeft, // Top-left corner
                ImVec2(viewportTopLeft.x + viewportSize.x, viewportTopLeft.y + viewportSize.y), // Bottom-right corner
                uv0,
                uv1
            );

            // --- 7. (THE FIX) Call ImGuizmo ---
            // Now that the 'er' resource is updated and the scene is drawn,
            // we call ImGuizmo. It will use the correct rect from 'er'
            // and draw on top of the background image.
            UpdateImGuizmo(ecs); // Assumes this is a member function of your class


            // --- 8. End Window ---
            ImGui::End();
            ImGui::PopStyleVar();
        }

        void UpdateImGuizmo(ECS* ecs)
        {
            auto er = ecs->GetResource<EditorResource>();
            if (!er || !er->selectedEntity) return;
            if (!er->selectedEntity.HasComponent<TransformComponent>()) return;
            TransformComponent& transformComponent = er->selectedEntity.GetComponent<TransformComponent>();

            glm::mat4 view = glm::mat4(1.0f);

            Entity cameraEntity = ecs->GetEntity("Camera");
            if (cameraEntity)
            {
                TransformComponent& tc = cameraEntity.GetComponent<TransformComponent>();
                CameraComponent& cc = cameraEntity.GetComponent<CameraComponent>();

                glm::vec3 cameraPos = tc.Translation;
                glm::vec3 forwardDir = glm::normalize(cc.Forward);
                glm::vec3 upDir = glm::normalize(cc.Up);
                glm::vec3 cameraTarget = cameraPos + forwardDir;
                view = glm::lookAt(cameraPos, cameraTarget, upDir);
            }

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), er->sceneWindowWidth / er->sceneWindowHeight, 0.1f, 100.0f);

            // Start setting up the guizmo
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(er->sceneWindowX, er->sceneWindowY, er->sceneWindowWidth, er->sceneWindowHeight);

            ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::UNIVERSAL;
            glm::mat4 modelMatrix = transformComponent.GetTransform();

            if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), operation, ImGuizmo::WORLD, glm::value_ptr(modelMatrix)))
            {
                glm::mat4 manipulatedTransform = modelMatrix;
                glm::vec3 newTranslation, newRotation, newScale;

                ImGuizmo::DecomposeMatrixToComponents(
                    glm::value_ptr(manipulatedTransform),
                    glm::value_ptr(newTranslation),
                    glm::value_ptr(newRotation),
                    glm::value_ptr(newScale));

                transformComponent.Translation = newTranslation;
                transformComponent.Rotation = glm::radians(newRotation);
                transformComponent.Scale = newScale;
            }
        }
    }
}
