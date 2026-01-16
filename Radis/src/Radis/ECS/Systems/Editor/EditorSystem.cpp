#include <PCH/pch.h>
#include "EditorSystem.h"
#include "Engine.h"
#include "ECS/ECS.h"
#include "ECS/Components/Components.h"

#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/WindowResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Resources/SerializationResource.h"
#include "ECS/Resources/SwapRendererResource.h"
#include "ECS/Systems/InputSystem.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Common/Animation/AnimationLibrary.h"
#include "Graphics/Common/Animation/Animation.h"
#include "Graphics/Common/ModelLibrary.h"
#include "Graphics/Common/Model.h"

#include "Graphics/Vulkan/VulkanWindow.h"
#include "Graphics/OpenGL/GLFrameBuffer.h"

#include "Windows/AssetsWindow.h"
#include "Windows/SceneWindow.h"
#include "Windows/EntitiesWindow.h"
#include "Windows/TextureBrowserWindow.h"
#include "Windows/ProfilerWindow.h"
#include "Windows/MemoryWindow.h"
#include "Windows/ChatWindow.h"
#include "Windows/Inspector/InspectorWindow.h"

#include "Utils/Utils.h"

#include "imgui_internal.h"

namespace Radis
{
    void EditorSystem::Init()
    {
    }

    void EditorSystem::FrameStart()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto er = ecs->GetResource<EditorResource>();

        // Start the Dear ImGui frame
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) ImGui_ImplVulkan_NewFrame();
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL) ImGui_ImplOpenGL3_NewFrame();

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        
        RenderMainMenuBar();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        auto& tl = ecs->GetResource<RenderingResource>()->textureLibrary;
        {
            PROFILE_SCOPE("Windows");

            EditorWindows::RenderSceneWindow(ecs);
            EditorWindows::RenderEntitiesWindow(ecs);
            EditorWindows::RenderTextureBrowser(ecs);
            EditorWindows::RenderProfilerWindow();
            EditorWindows::RenderMemoryWindow();
            EditorWindows::UpdateAssetsWindow(tl.get());
            ChatWindow::Get().Render();
            EditorWindows::RenderInspectorWindow(ecs);
        }


        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Checkbox("Wireframe", &rr->renderWireframe);
        if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            glPolygonMode(GL_FRONT_AND_BACK, rr->renderWireframe ? GL_LINE : GL_FILL);
        }
        
        ImGui::BeginDisabled(Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan);
        ImGui::Checkbox("Raytracing", &rr->useRaytracing);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!rr->useRaytracing);
        ImGui::Checkbox("Raytracing Heatmap Estimation", &er->renderRaytracingHeatmap);
        ImGui::EndDisabled();
        ImGui::End();

        // Handle mouse lock for ImGui windows (excluding "Viewport")
        {
            ImGuiIO& io = ImGui::GetIO();

            // If left mouse button just pressed
            if (InputSystem::isMouseDown(Mouse::LEFT))
            {
                ImGuiWindow* hoveredWindow = ImGui::GetCurrentContext()->HoveredWindow;
                if (hoveredWindow && strcmp(hoveredWindow->Name, "Viewport") != 0)
                {
                    mLockMouse = true;
                }
            }

            // If left mouse released -> unlock
            if (!InputSystem::isMouseDown(Mouse::LEFT))
            {
                mLockMouse = false;
            }

            InputSystem::SetMouseInputLocked(mLockMouse);
        }

        if (er->selectedEntity)
        {
            if (ImGui::GetIO().KeyCtrl && InputSystem::isKeyTriggered(Key::D))
            {
                Entity newEntity = ecs->CloneEntity(er->selectedEntity);
                er->selectedEntity = newEntity;
            }
        }

        // If clicking anywhere outside "Entities" window, deselect entity
    }

    void EditorSystem::Update(float dt)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            if (!rr)
            {
                RADIS_CRITICAL("No rendering resource in editor system");
                return;
            }

            rr->renderGraph->AddPass(
                "ImGuiPass",
                [&](RGPassBuilder& builder) {
                    builder.reads("SceneColor");
                    builder.writes("BackBuffer");
                },
                std::bind(&EditorSystem::RenderImGui, this, std::placeholders::_1)
            );
        }

        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            RenderImGui();
        }
    }

    void EditorSystem::FrameEnd()
    {
    }

    void EditorSystem::Exit()
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) {
            auto rr = ecs->GetResource<RenderingResource>();
            auto er = ecs->GetResource<EditorResource>();
            Device& device = *rr->device;

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            vkDestroyDescriptorSetLayout(device, er->samplerSetLayout, nullptr);
            vkDestroyDescriptorPool(device, er->descriptorPool, nullptr);
        }
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
    }

	void EditorSystem::RenderImGui(VkCommandBuffer cmd)
	{
		// Rendering
		ImGui::Render();

        if      (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL) ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

    void EditorSystem::RenderMainMenuBar()
    {
        // persistent across frames
        static bool requestOpenSavePopup = false;
        static char sceneNameBuffer[128] = "";

        if (!ImGui::BeginMainMenuBar())
            return;

        // -----------------------
        // File Menu
        // -----------------------
        if (ImGui::BeginMenu("File"))
        {
            // Save
            if (ImGui::MenuItem("Save Scene"))
            {
                auto sr = ecs->GetResource<SerializationResource>();
                sr->Serialize(sr->currentScenePath);
            }

            // Save As
            if (ImGui::MenuItem("Save Scene As..."))
            {
                // Prepare buffer with current scene name (best guess)
                auto sr = ecs->GetResource<SerializationResource>();
                try {
                    std::filesystem::path p(sr->currentScenePath);
                    auto stem = p.stem().string();
                    strncpy(sceneNameBuffer, stem.c_str(), sizeof(sceneNameBuffer) - 1);
                    sceneNameBuffer[sizeof(sceneNameBuffer) - 1] = '\0';
                }
                catch (...) {
                    sceneNameBuffer[0] = '\0';
                }

                requestOpenSavePopup = true;
            }

            // Open Scene menu
            if (ImGui::BeginMenu("Open Scene"))
            {
                const std::filesystem::path sceneDir = Assets::ScenesPath;
                if (std::filesystem::exists(sceneDir) && std::filesystem::is_directory(sceneDir))
                {
                    for (auto const& entry : std::filesystem::directory_iterator(sceneDir))
                    {
                        if (!entry.is_regular_file()) continue;
                        if (entry.path().extension() != ".json") continue;

                        auto file = entry.path().filename().string();
                        if (ImGui::MenuItem(file.c_str()))
                        {
                            ecs->GetResource<SerializationResource>()->Deserialize((sceneDir / file).string());
                        }
                    }
                }
                else
                {
                    ImGui::MenuItem("(no scenes found)", nullptr, false, false);
                }
                
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Graphics"))
        {
            auto currentAPI = Engine::GetGraphicsAPI();
            auto sr = ecs->GetResource<SwapRendererResource>();

            if (ImGui::MenuItem("Vulkan", nullptr, currentAPI == GraphicsAPI::Vulkan)) sr->RequestVulkan();
            if (ImGui::MenuItem("OpenGL", nullptr, currentAPI == GraphicsAPI::OpenGL)) sr->RequestOpenGL();

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();

        // Save popup
        if (requestOpenSavePopup)
        {
            ImGui::OpenPopup("Save Scene As");
            requestOpenSavePopup = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5f, 0.5f });

        if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Enter Scene Name:");
            ImGui::InputText("##SceneNameInput", sceneNameBuffer, IM_ARRAYSIZE(sceneNameBuffer));

            ImGui::Separator();

            // Save button
            if (ImGui::Button("Save"))
            {
                std::string name(sceneNameBuffer);

                // simple whitespace check
                bool valid = false;
                for (char c : name)
                    if (!isspace((unsigned char)c)) { valid = true; break; }

                if (valid)
                {
                    std::string filename = name;
                    if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".json")
                    {
                        filename += ".json";
                    }
                    
                    auto sr = ecs->GetResource<SerializationResource>();
                    sr->Serialize((std::filesystem::path(Assets::ScenesPath) / filename).string());

                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();

            // Cancel button
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}