/*****************************************************************//**
 * \file   EntitiesWindow.cpp
 * \brief  Renders the Entities Window!
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "EntitiesWindow.h"

#include "ECS/Resources/EditorResource.h"
#include "ECS/Components/Components.h"
#include "ECS/ECS.h"

namespace Radis
{
    namespace EditorWindows
    {
        void RenderEntitiesWindow(ECS* ecs)
        {
            PROFILE_SCOPE("Entities");

            auto er = ecs->GetResource<EditorResource>();
            if (!er) return;

            entt::registry& registry = ecs->GetRegistry();

            ImGui::Begin("Entities##window");

            static char entityFilter[256] = "";

            float buttonWidth = 45.0f;
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth);
            ImGui::InputTextWithHint("##EntityFilter", "Search...", entityFilter, IM_ARRAYSIZE(entityFilter));
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("+"))
            {
                Entity newEntity = ecs->AddEntity("New Entity");
                er->selectedEntities = { newEntity };
            }
            if (ImGui::Button("LightTest+"))
            {
                int n = 25;
                for (int x = 0; x < n; x++)
                {
                    for (int y = 0; y < n; y++)
                    {
                        for (int z = 0; z < n; z++)
                        {
                            Entity debugEntity = ecs->AddEntity("l" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z));
                            auto& transform = debugEntity.GetComponent<TransformComponent>();
                            transform.SetTranslation((float)x * 2.f, (float)y * 2.f, (float)z * 2.f);

                            // add model
                            auto& mc = debugEntity.AddComponent<ModelComponent>("assets/models/sphere.obj");
                            
                            // add light
                            auto& lc = debugEntity.AddComponent<LightComponent>();
                            lc.Radius = 10.f;

                            // Color based on position
                            lc.Color = glm::vec3(
                                (float)x / (float)n,
                                (float)y / (float)n,
                                (float)z / (float)n
                            );
                        }
                    }
                }
            }
            if (ImGui::Button("SpamTest+"))
            {
                int n = 75;
                for (int x = 0; x < n; x++)
                {
                    for (int y = 0; y < n; y++)
                    {
                        for (int z = 0; z < n; z++)
                        {
                            Entity debugEntity = ecs->AddEntity("l" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z));
                            auto& transform = debugEntity.GetComponent<TransformComponent>();
                            
                            // add little offsets in tx ty and tz to make it not completely uniform 
                            float tx = ((float)x - (float)(n / 2)) * 1.5f + (glm::fract(glm::sin((float)(x * y * z + 1)) * 43758.5453f) - 0.5f);
                            float ty = ((float)y - (float)(n / 2)) * 1.5f + (glm::fract(glm::sin((float)(x * y * z + 2)) * 43758.5453f) - 0.5f);
                            float tz = ((float)z - (float)(n / 2)) * 1.5f + (glm::fract(glm::sin((float)(x * y * z + 3)) * 43758.5453f) - 0.5f);
                            transform.SetTranslation(tx, ty, tz);
                            transform.SetScale(0.5f);

                            // add model
                            auto& mc = debugEntity.AddComponent<ModelComponent>("assets/models/cube.obj");
                            mc.TintColor = glm::vec4(
                                (float)x / (float)n,
                                (float)y / (float)n,
                                (float)z / (float)n,
                                1.0f
                            );
                        }
                    }
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Add New Entity");
            }

            ImGui::Separator();
            ImGui::BeginChild("EntityListRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            std::string filterLower;
            if (entityFilter[0] != '\0')
            {
                filterLower = entityFilter;
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return std::tolower(c); });
            }

            bool anyEntityClicked = false;

            auto view = registry.view<TagComponent>();
            for (auto [entityHandle, tag] : view.each())
            {
                if (entityFilter[0] != '\0')
                {
                    std::string tagLower = tag.Tag;
                    std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), [](unsigned char c) { return std::tolower(c); });

                    if (tagLower.find(filterLower) == std::string::npos)
                    {
                        continue; // Skip this entity
                    }
                }

                Entity entity(&registry, entityHandle);

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                
                bool isEntitySelected = false;
                for (const auto& selectedEntity : er->selectedEntities)
                {
                    if (selectedEntity == entity)
                    {
                        isEntitySelected = true;
                        break;
                    }
                }

                if (isEntitySelected)
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entityHandle, flags, tag.Tag.c_str());

                // --- Handle Interactions ---
                // Since we can handle selecting many entities, handle ctrl click, shift click, and regular click
                if (ImGui::IsItemClicked())
                {
                    if (ImGui::GetIO().KeyCtrl) // Ctrl + Click for multi-select
                    {
                        bool alreadySelected = false;
                        for (const auto& selectedEntity : er->selectedEntities)
                        {
                            if (selectedEntity == entity)
                            {
                                alreadySelected = true;
                                break;
                            }
                        }

                        if (alreadySelected)
                        {
                            // Deselect
                            er->selectedEntities.erase(std::remove(er->selectedEntities.begin(), er->selectedEntities.end(), entity), er->selectedEntities.end());
                        }
                        else
                        {
                            // Add to selection
                            er->selectedEntities.push_back(entity);
                        }
                    }
                    else if (ImGui::GetIO().KeyShift) // Shift + Click for range select
                    {
                        if (!er->selectedEntities.empty())
                        {
                            auto lastSelected = er->selectedEntities.back();
                            er->selectedEntities.clear();
                            bool inRange = false;
                            for (auto [eHandle, eTag] : view.each())
                            {
                                Entity currentEntity(&registry, eHandle);
                                if (currentEntity == lastSelected || currentEntity == entity)
                                {
                                    if (!inRange)
                                    {
                                        inRange = true;
                                    }
                                    else
                                    {
                                        er->selectedEntities.push_back(currentEntity);
                                        break;
                                    }
                                }
                                if (inRange)
                                {
                                    er->selectedEntities.push_back(currentEntity);
                                }
                            }
                        }
                        else
                        {
                            er->selectedEntities = { entity };
                        }
                    }
                    else // Regular click
                    {
                        er->selectedEntities = { entity };
                    }
                    anyEntityClicked = true;
                }
                
                // if (ImGui::IsItemClicked())
                // {
                //     er->selectedEntities = { entity };
                //     anyEntityClicked = true;
                // }

                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Remove Entity"))
                    {
                        er->entitiesToDelete.push_back(entity);
                    }
                    ImGui::EndPopup();
                }

                // --- Cleanup ---
                if (opened)
                {
                    ImGui::TreePop();
                }
            }

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !anyEntityClicked)
            {
                er->selectedEntities.clear(); // Reset selected entity
            }

            ImGui::EndChild(); // End of "EntityListRegion"
            ImGui::End(); // End of "Entities##window"
        }
    }
}
