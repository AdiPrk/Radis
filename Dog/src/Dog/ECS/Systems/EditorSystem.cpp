#include <PCH/pch.h>
#include "EditorSystem.h"
#include "Engine.h"
#include "../ECS.h"
#include "../Entities/Components.h"

#include "../Resources/RenderingResource.h"
#include "../Resources/WindowResource.h"
#include "../Resources/EditorResource.h"
#include "../Resources/SerializationResource.h"
#include "../Systems/InputSystem.h"

#include "Graphics/Vulkan/Core/Device.h"
#include "Graphics/Vulkan/Core/SwapChain.h"
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Model/Animation/AnimationLibrary.h"
#include "Graphics/Vulkan/Model/Animation/Animation.h"
#include "Graphics/Vulkan/Model/ModelLibrary.h"
#include "Graphics/Vulkan/Model/Model.h"

#include "Graphics/Vulkan/VulkanWindow.h"

#include "imgui_internal.h"

namespace Dog
{
    void EditorSystem::Init()
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto rr = ecs->GetResource<RenderingResource>();
		    rr->RecreateAllSceneTextures();
        }
    }

    void EditorSystem::FrameStart()
    {
    }

    void EditorSystem::Update(float dt)
    {
        if (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            if (!rr)
            {
                DOG_CRITICAL("No rendering resource in editor system");
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
            RenderImGui(VK_NULL_HANDLE);
        }
    }

    void EditorSystem::FrameEnd()
    {
        auto er = ecs->GetResource<EditorResource>();
        if (er->entityToDelete)
        {
            if (er->selectedEntity == er->entityToDelete)
            {
                er->selectedEntity = {};
            }

            ecs->RemoveEntity(er->entityToDelete);
            er->entityToDelete = {};
        }
    }

    void EditorSystem::Exit()
    {
        auto rr = ecs->GetResource<RenderingResource>();
        auto er = ecs->GetResource<EditorResource>();
        Device& device = *rr->device;

        if      (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) ImGui_ImplVulkan_Shutdown();
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL) ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		vkDestroyDescriptorSetLayout(device, er->samplerSetLayout, nullptr);
		vkDestroyDescriptorPool(device, er->descriptorPool, nullptr);

		rr->CleanupSceneTexture();
        rr->CleanupDepthBuffer();
    }

	void EditorSystem::RenderImGui(VkCommandBuffer cmd)
	{
        auto er = ecs->GetResource<EditorResource>();

        // Start the Dear ImGui frame
        if      (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) ImGui_ImplVulkan_NewFrame();
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL) ImGui_ImplOpenGL3_NewFrame();

        ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

        RenderMainMenuBar();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		
        RenderSceneWindow();
		RenderEntitiesWindow(); 
		RenderInspectorWindow();

        ImGui::Begin("Debug");
        //ImGui::Checkbox("Wireframe", &ecs->GetResource<RenderingResource>()->renderWireframe);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
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
            // if ctrl + d
            if (ImGui::GetIO().KeyCtrl && InputSystem::isKeyTriggered(Key::D))
            {
                Entity newEntity = ecs->CloneEntity(er->selectedEntity);
                er->selectedEntity = newEntity;
            }
        }

		// Rendering
		ImGui::Render();

        if      (Engine::GetGraphicsAPI() == GraphicsAPI::Vulkan) ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        else if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL) ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

    void EditorSystem::RenderMainMenuBar()
    {
        if (!ImGui::BeginMainMenuBar()) return;
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene"))
                {
                    ecs->GetResource<SerializationResource>()->Serialize("assets/scenes/scene.json");
                }
                if (ImGui::MenuItem("Load Scene"))
                {
                    ecs->GetResource<SerializationResource>()->Deserialize("assets/scenes/scene.json");
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void EditorSystem::RenderSceneWindow()
	{
        if (Engine::GetGraphicsAPI() == GraphicsAPI::OpenGL)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("Viewport");
            ImGui::Text("Cannot render Scene Viewport in OpenGL yet.");
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }
            
		// Create a window and display the scene texture
		VkDescriptorSet sceneTextureDescriptorSet = ecs->GetResource<RenderingResource>()->sceneTextureDescriptorSet;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		ImGui::Image(reinterpret_cast<void*>(sceneTextureDescriptorSet), viewportSize);
		ImGui::End();
        ImGui::PopStyleVar();

		auto editorResource = ecs->GetResource<EditorResource>();
		editorResource->sceneWindowWidth = viewportSize.x;
		editorResource->sceneWindowHeight = viewportSize.y;
	}

	void EditorSystem::RenderEntitiesWindow()
	{
        auto er = ecs->GetResource<EditorResource>();

		ImGui::Begin("Entities##window", nullptr, ImGuiWindowFlags_MenuBar);
		entt::registry& registry = ecs->GetRegistry();
		auto view = registry.view<TagComponent>();

		for (auto& entityHandle : view) 
		{
			Entity entity(&registry, entityHandle);
			TagComponent& tag = entity.GetComponent<TagComponent>();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (er->selectedEntity == entity)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entityHandle, flags, tag.Tag.c_str());
            if (ImGui::IsItemClicked())
            {
                er->selectedEntity = entity;
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove Entity"))
                {
                    // Mark for deletion, but don't delete yet!
                    er->entityToDelete = entity;
                }
                ImGui::EndPopup();
            }

            if (opened)
            {
                ImGui::TreePop();
            }
		}

		ImGui::End(); // End of Entities
	}

    template<typename T, typename UIFunction>
    static void DrawComponentUI(const char* name, Entity entity, UIFunction uiFunction)
    {
        // Ensure the entity has the component before proceeding
        if (!entity.HasComponent<T>())
        {
            return;
        }

        const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen
                                               | ImGuiTreeNodeFlags_Framed
                                               | ImGuiTreeNodeFlags_SpanAvailWidth
                                               | ImGuiTreeNodeFlags_AllowItemOverlap
                                               | ImGuiTreeNodeFlags_FramePadding;

        T& component = entity.GetComponent<T>();

        // Create a collapsible header for the component
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
        float lineHeight = ImGui::GetFrameHeight();
        ImGui::Separator();
        bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name);
        ImGui::PopStyleVar();

        // --- Component Settings Menu ---
        ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
        ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
        if (ImGui::Button("...", ImVec2{ lineHeight, lineHeight }))
        {
            ImGui::OpenPopup("ComponentSettings");
        }

        bool componentRemoved = false;
        if (ImGui::BeginPopup("ComponentSettings"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                componentRemoved = true;
            }
            ImGui::EndPopup();
        }

        // If the collapsible header is open, draw the component's properties
        if (open)
        {
            uiFunction(component);
            ImGui::TreePop();
        }

        // Remove component if needed
        if (componentRemoved)
        {
            // Avoid removal of Tag and Transform
            if constexpr (!std::is_same_v<T, TagComponent> && !std::is_same_v<T, TransformComponent>)
            {
                entity.RemoveComponent<T>();
            }
        }
    }

    // Helper for the "Add Component" dropdown
    template<typename T>
    void DisplayAddComponentEntry(const char* name, Entity entity)
    {
        if (!entity.HasComponent<T>())
        {
            if (ImGui::MenuItem(name))
            {
                entity.AddComponent<T>();
                ImGui::CloseCurrentPopup();
            }
        }
    }

    static std::vector<std::string> GetFilesWithExtensions(const std::string& directoryPath, const std::vector<std::string>& extensions)
    {
        std::vector<std::string> fileNames;
        try
        {
            // Check if the directory exists
            if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath))
            {
                // You could log an error here if you want
                return fileNames;
            }

            // Iterate over each entry in the directory
            for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
            {
                if (entry.is_regular_file())
                {
                    // Get the file extension
                    std::string extension = entry.path().extension().string();

                    // Convert extension to lower case for case-insensitive comparison
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                    // Check if the file's extension is in our list of desired extensions
                    for (const auto& desiredExt : extensions)
                    {
                        if (extension == desiredExt)
                        {
                            // Add the filename to our list
                            fileNames.push_back(entry.path().string());
                            break; // Move to the next file
                        }
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            // Handle potential exceptions, e.g., permission errors
            // For now, we can just print to stderr
            fprintf(stderr, "Filesystem error: %s\n", e.what());
        }

        return fileNames;
    }

    // --- Main Inspector Window Function ---
    void EditorSystem::RenderInspectorWindow()
    {
        ImGui::Begin("Inspector");

        Entity selectedEnt = ecs->GetResource<EditorResource>()->selectedEntity;

        // Handle the case where no entity is selected
        if (!selectedEnt)
        {
            ImGui::Text("No entity selected.");
            ImGui::End();
            return;
        }

        // --- Tag Component (Special case, always at the top) ---
        if (selectedEnt.HasComponent<TagComponent>())
        {
            auto& tag = selectedEnt.GetComponent<TagComponent>();
            ImGui::InputText("##Tag", &tag.Tag);
        }

        // --- Scrolling Component Region ---
        // We create a child window to house the components. This allows the component list to
        // scroll while the footer with the 'Add Component' and 'Add Entity' buttons remains fixed.
        // The negative height tells ImGui to use all available space minus the specified amount.
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2.2f;
        ImGui::BeginChild("ComponentsRegion", ImVec2(0, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

        DrawComponentUI<TransformComponent>("Transform", selectedEnt, [](auto& component)
        {
            // Displaying rotation in degrees
            glm::vec3 rotationDegrees = glm::degrees(component.Rotation);

            ImGui::DragFloat3("Translation", glm::value_ptr(component.Translation), 0.1f);
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), 0.1f))
            {
                component.Rotation = glm::radians(rotationDegrees);
            }
            ImGui::DragFloat3("Scale", glm::value_ptr(component.Scale), 0.1f);
        });

        DrawComponentUI<CameraComponent>("CameraComponent", selectedEnt, [](auto& component)
        {
            // Displaying rotation in degrees
            ImGui::DragFloat("FOV", &component.FOV, 0.1f, 1.0f, 120.0f);
            ImGui::DragFloat("Near", &component.Near, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Far", &component.Far, 1.0f, 10.0f, 10000.0f);
            ImGui::DragFloat("Move Speed", &component.MoveSpeed, 0.1f, 0.1f, 100.0f);
            ImGui::DragFloat("Mouse Sensitivity", &component.MouseSensitivity, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Yaw", &component.Yaw, 0.1f, -360.0f, 360.0f);
            ImGui::DragFloat("Pitch", &component.Pitch, 0.1f, -89.0f, 89.0f);
        });

        DrawComponentUI<ModelComponent>("Model", selectedEnt, [&](ModelComponent& component)
        {
            const std::vector<std::string> modelExtensions = { ".fbx", ".glb", ".obj" };
            const std::vector<std::string>& modelFiles = GetFilesWithExtensions("assets/models/", modelExtensions);

            auto rr = ecs->GetResource<RenderingResource>();
            auto& mc = rr->modelLibrary;

            Model* currentModel = mc->GetModel(component.ModelPath);
            std::string currPath = currentModel ? currentModel->GetName() : "None";

            if (ImGui::BeginCombo("Model", currPath.c_str()))
            {
                for (int i = 0; i < modelFiles.size(); ++i)
                {
                    const std::string& modelPath = modelFiles[i];
                    const bool isSelected = currPath == modelPath;
                    if (ImGui::Selectable(modelPath.c_str(), isSelected))
                    {
                        std::string lowerModelPath = modelPath;
                        std::transform(lowerModelPath.begin(), lowerModelPath.end(), lowerModelPath.begin(), ::tolower);

                        component.ModelPath = lowerModelPath;
                    }
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }

                }
                
                ImGui::EndCombo();
            }

            ImGui::ColorEdit4("Tint Color", glm::value_ptr(component.tintColor));
        });

        auto rr = ecs->GetResource<RenderingResource>();
        auto& animationLibrary = rr->animationLibrary;

        static int selectedAnimationIndex = -1;

        DrawComponentUI<AnimationComponent>("Animation", selectedEnt, [&](AnimationComponent& component)
        {
            Entity ent(&ecs->GetRegistry(), selectedEnt);
            bool hasModel = ent.HasComponent<ModelComponent>();

            if (!hasModel)
            {
                ImGui::Text("No model assigned to entity for animations!");
                return;
            }

            const auto& mc = ent.GetComponent<ModelComponent>();
            Model* model = rr->modelLibrary->GetModel(mc.ModelPath);
            if (!model)
            {
                ImGui::Text("Invalid model for animations!");
                return;
            }

            const std::string animationsPath = model->GetDir();
            const std::vector<std::string> animationExtensions = { ".fbx", ".glb" };
            auto animationFiles = GetFilesWithExtensions(animationsPath, animationExtensions);

            // --- Animation Selection Dropdown ---
            const auto& animName = animationLibrary->GetAnimationName(component.AnimationIndex);
            std::string cutName = animName;
            if (animName.find('|') != std::string::npos)
            {
                cutName = animName.substr(animName.find('|') + 1);
            }

            if (ImGui::BeginCombo("Animation", cutName.c_str()))
            {
                for (int i = 0; i < animationFiles.size(); ++i)
                {
                    const bool isSelected = (selectedAnimationIndex == i);
                    if (ImGui::Selectable(animationFiles[i].c_str(), isSelected))
                    {
                        selectedAnimationIndex = i;

                        // remove the first part up to the "|" from animName
                        uint32_t animationIndex = animationLibrary->GetAnimationIndex(model->GetName(), animationFiles[i]);
                        component.AnimationIndex = animationIndex;

                        if (animationIndex == AnimationLibrary::INVALID_ANIMATION_INDEX)
                        {
                            std::string newName = animationFiles[i];
                            uint32_t newIndex = animationLibrary->AddAnimation(animationsPath + newName, model);
                            component.AnimationIndex = newIndex;
                        }
                    }
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }            

            ImGui::InputInt("Animation Index", (int*)&component.AnimationIndex);
            ImGui::Checkbox("In Place", &component.inPlace);
            ImGui::Checkbox("Is Playing", &component.IsPlaying);
            ImGui::DragFloat("Animation Time", &component.AnimationTime, 0.05f, 0.0f, FLT_MAX);
        });

        ImGui::EndChild(); // End of ComponentsRegion

        // --- Fixed Footer Region ---
        ImGui::Separator();

        // Button to add a new component to the selected entity
        if (ImGui::Button("Add Component", ImVec2(-1, 0)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            DisplayAddComponentEntry<ModelComponent>("Model", selectedEnt);
            DisplayAddComponentEntry<AnimationComponent>("Animation", selectedEnt);
            // Add more component types here!
            ImGui::EndPopup();
        }

        // Button to add a new entity to the scene
        if (ImGui::Button("Add Entity", ImVec2(-1, 0)))
        {
            Entity newEntity = ecs->AddEntity("New Entity");
            // Automatically select the new entity for immediate editing
            ecs->GetResource<EditorResource>()->selectedEntity = newEntity;
        }

        ImGui::End(); // End of Inspector window
    }
}