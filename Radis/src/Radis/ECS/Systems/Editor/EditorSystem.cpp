/*****************************************************************//**
 * \file   EditorSystem.cpp
 * \brief  Manages the Editor
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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
#include "Windows/InspectorWindow.h"

#include "Utils/Utils.h"

#include "imgui_internal.h"

namespace Radis
{
    void SetupImGuiStyle()
    {
        // Future Dark style by rewrking from ImThemes
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha = 1.0f;
        style.DisabledAlpha = 1.0f;
        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.WindowRounding = 0.0f;
        style.WindowBorderSize = 0.0f;
        style.WindowMinSize = ImVec2(20.0f, 20.0f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ChildRounding = 0.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(6.0f, 6.0f);
        style.FrameRounding = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(12.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 3.0f);
        style.CellPadding = ImVec2(12.0f, 6.0f);
        style.IndentSpacing = 20.0f;
        style.ColumnsMinSpacing = 6.0f;
        style.ScrollbarSize = 12.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabMinSize = 12.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.TabBorderSize = 0.0f;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.27450982f, 0.31764707f, 0.4509804f, 1.0f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.23529412f, 0.21568628f, 0.59607846f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.5372549f, 0.5529412f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.23529412f, 0.21568628f, 0.59607846f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.23529412f, 0.21568628f, 0.59607846f, 1.0f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.23529412f, 0.21568628f, 0.59607846f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.52156866f, 0.6f, 0.7019608f, 1.0f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.039215688f, 0.98039216f, 0.98039216f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(1.0f, 0.2901961f, 0.59607846f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.99607843f, 0.4745098f, 0.69803923f, 1.0f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.23529412f, 0.21568628f, 0.59607846f, 1.0f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
    }

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
        
        SetupImGuiStyle();

        RenderMainMenuBar();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        auto& tl = ecs->GetResource<RenderingResource>()->textureLibrary;
        {
            PROFILE_SCOPE("Windows");

            if (EditorWindows::RenderTextureBrowser(ecs))
            {
                float flipY = static_cast<float>(Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL);
                EditorWindows::RenderFullscreenViewer(tl.get(), flipY);
            }
            else 
            {
                EditorWindows::RenderSceneWindow(ecs);
                EditorWindows::RenderEntitiesWindow(ecs);
                EditorWindows::RenderProfilerWindow();
                EditorWindows::RenderMemoryWindow();
                EditorWindows::UpdateAssetsWindow(tl.get());
                ChatWindow::Get().Render();
                EditorWindows::RenderInspectorWindow(ecs);
                RenderDebugWindow();
            }
        }

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

        if (!er->selectedEntities.empty() && ImGui::GetIO().KeyCtrl && InputSystem::isKeyTriggered(Key::D))
        {
            // clone all entities
            std::vector<Entity> newEntities;
            newEntities.reserve(er->selectedEntities.size());

            for (const auto& entity : er->selectedEntities)
            {
                Entity newEntity = ecs->CloneEntity(entity);
                newEntities.push_back(newEntity);
            }

            er->selectedEntities = newEntities;
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
                [&](RGPassBuilder& b) {
                    b.reads("SceneTexture");
                    b.reads("SceneDepth");
                    b.reads("ShadowMomentsRaw");
                    b.reads("ShadowMomentsTmp");
                    b.reads("ShadowMoments");
                    b.reads("ShadowDepth");
                    b.reads("gAlbedo");
                    b.reads("gNormal");
                    b.reads("gPBR");
                    b.reads("gEmissive");
                    b.reads("SceneHDR");
                    b.writes("BackBuffer"); 
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
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            // glDisable(GL_FRAMEBUFFER_SRGB);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            // glEnable(GL_FRAMEBUFFER_SRGB);
        }
	}

    void EditorSystem::RenderDebugWindow()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto er = ecs->GetResource<EditorResource>();

        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Checkbox("Wireframe", &rr->renderWireframe);
        if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            glPolygonMode(GL_FRONT_AND_BACK, rr->renderWireframe ? GL_LINE : GL_FILL);
        }

        ImGui::BeginDisabled(Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan);

        ImGui::Text("Render Mode:");
        const char* renderModeItems[] = { "Forward", "Deferred", "Raytracing" };
        int currentRenderMode = static_cast<int>(rr->renderMode);
        if (ImGui::Combo("##RenderMode", &currentRenderMode, renderModeItems, IM_ARRAYSIZE(renderModeItems)))
        {
            rr->renderMode = static_cast<RenderMode>(currentRenderMode);
        }

        ImGui::EndDisabled();

        ImGui::BeginDisabled(rr->renderMode != RenderMode::Raytracing);
        ImGui::Checkbox("Raytracing Heatmap Estimation", &er->renderRaytracingHeatmap);
        ImGui::EndDisabled();

        // lightVolumeDebugMode, combo of (0=normal, 1=volume tint, 2=density heatmap)
        ImGui::Text("Light Volume Debug Mode:");
        const char* lightVolumeDebugModeItems[] = { "Normal", "Volume Tint", "Density Heatmap" };
        int currentLightVolumeDebugMode = rr->lightVolumeDebugMode;
        if (ImGui::Combo("##LightVolumeDebugMode", &currentLightVolumeDebugMode, lightVolumeDebugModeItems, IM_ARRAYSIZE(lightVolumeDebugModeItems)))
        {
            rr->lightVolumeDebugMode = currentLightVolumeDebugMode;
        }

        ImGui::DragInt("MSM Blur Radius", &rr->msmPC.radius, 0.1f, 0, 16);
        ImGui::DragFloat("MSM Blur Sigma", &rr->msmPC.sigma, 0.1f, 0.0f, 8.0f);

        ImGui::End();
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