#include <PCH/pch.h>
#include "EntitiesWindow.h"

#include "ECS/Resources/EditorResource.h"
#include "ECS/Entities/Components.h"
#include "ECS/ECS.h"

namespace Dog
{
    namespace EditorWindows
    {
        void RenderEntitiesWindow(ECS* ecs)
        {
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
                er->selectedEntity = newEntity;
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
                if (er->selectedEntity == entity)
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entityHandle, flags, tag.Tag.c_str());

                // --- Handle Interactions ---
                if (ImGui::IsItemClicked())
                {
                    er->selectedEntity = entity;
                    anyEntityClicked = true;
                }

                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Remove Entity"))
                    {
                        er->entityToDelete = entity;
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
                er->selectedEntity = {}; // Reset selected entity
            }

            ImGui::EndChild(); // End of "EntityListRegion"
            ImGui::End(); // End of "Entities##window"
        }
    }
}
