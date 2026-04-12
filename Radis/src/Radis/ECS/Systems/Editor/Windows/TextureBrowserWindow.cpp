/*****************************************************************//**
 * \file   TextureBrowserWindow.cpp
 * \brief  Renders the Texture Browser!
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "TextureBrowserWindow.h"

#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/OpenGL/GLFrameBuffer.h"

#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Components/Components.h"
#include "Engine.h"

namespace Radis
{
    namespace EditorWindows
    {
        // Static state for fullscreen viewer
        static bool sFullscreenMode = false;
        static int32_t sSelectedTextureIndex = -1;
        static float sZoomLevel = 1.0f;
        static ImVec2 sPanOffset = { 0.0f, 0.0f };
        static bool sDragging = false;
        static ImVec2 sDragStart = { 0.0f, 0.0f };
        static bool sFlipY = false;

        void RenderFullscreenViewer(TextureLibrary* tl, float flipY)
        {
            const uint32_t textureCount = tl->GetTextureCount();
            if (sSelectedTextureIndex < 0 || sSelectedTextureIndex >= static_cast<int32_t>(textureCount))
            {
                sFullscreenMode = false;
                return;
            }

            ITexture* texture = tl->GetTextureByIndex(static_cast<uint32_t>(sSelectedTextureIndex));
            if (!texture)
            {
                sFullscreenMode = false;
                return;
            }

            float actualFlipY = sFlipY ? (1.0f - flipY) : flipY;
            ImVec2 uv0 = { 0.0f, actualFlipY };
            ImVec2 uv1 = { 1.0f, 1.0f - actualFlipY };

            bool windowOpen = true;
            ImGui::Begin("Texture Viewer", &windowOpen);

            if (!windowOpen)
            {
                sFullscreenMode = false;
                sZoomLevel = 1.0f;
                sPanOffset = { 0.0f, 0.0f };
                ImGui::End();
                return;
            }

            // Handle keyboard input
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            {
                // ESC or Backspace to close
                if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Backspace))
                {
                    sFullscreenMode = false;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
                // Left/Right arrows or A/D to navigate
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_A))
                {
                    sSelectedTextureIndex = (sSelectedTextureIndex - 1 + textureCount) % textureCount;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_D))
                {
                    sSelectedTextureIndex = (sSelectedTextureIndex + 1) % textureCount;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
                // R to reset zoom/pan
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                {
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
                // Y to flip image vertically
                if (ImGui::IsKeyPressed(ImGuiKey_Y))
                {
                    sFlipY = !sFlipY;
                }
            }

            // Top bar with controls
            ImGui::BeginChild("TopBar", ImVec2(0, 40), false, ImGuiWindowFlags_NoScrollbar);
            {
                // Back button
                if (ImGui::Button("<< Back (ESC)"))
                {
                    sFullscreenMode = false;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
                ImGui::SameLine();

                // Previous button
                if (ImGui::Button("< Prev (A)"))
                {
                    sSelectedTextureIndex = (sSelectedTextureIndex - 1 + textureCount) % textureCount;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
                ImGui::SameLine();

                // Next button
                if (ImGui::Button("Next (D) >"))
                {
                    sSelectedTextureIndex = (sSelectedTextureIndex + 1) % textureCount;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
                ImGui::SameLine();

                // Texture name and index
                ImGui::Text("  [%d/%d] %s", sSelectedTextureIndex + 1, textureCount, texture->mData.name.c_str());
                ImGui::SameLine();

                // Texture dimensions
                ImGui::Text("  (%dx%d)", texture->mData.width, texture->mData.height);
                ImGui::SameLine();

                // Zoom controls
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 250);
                ImGui::Text("Zoom:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::SliderFloat("##Zoom", &sZoomLevel, 0.1f, 10.0f, "%.1fx");
                ImGui::SameLine();
                if (ImGui::Button("Reset (R)"))
                {
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
            }
            ImGui::EndChild();

            ImGui::Separator();

            // Image display area — leave room at the bottom for the help text
            const float helpBarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            ImGui::BeginChild("ImageArea", ImVec2(0, -helpBarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                ImVec2 availSize = ImGui::GetContentRegionAvail();
                float texWidth = static_cast<float>(texture->mData.width);
                float texHeight = static_cast<float>(texture->mData.height);

                // Calculate base size to fit in available area while maintaining aspect ratio
                float scaleX = availSize.x / texWidth;
                float scaleY = availSize.y / texHeight;
                float baseScale = std::min(scaleX, scaleY) * 0.9f; // 90% of available space

                float displayWidth = texWidth * baseScale * sZoomLevel;
                float displayHeight = texHeight * baseScale * sZoomLevel;

                // Center the image with pan offset
                ImVec2 imagePos;
                imagePos.x = (availSize.x - displayWidth) * 0.5f + sPanOffset.x;
                imagePos.y = (availSize.y - displayHeight) * 0.5f + sPanOffset.y;

                ImGui::SetCursorPos(imagePos);

                // Handle mouse wheel zoom
                if (ImGui::IsWindowHovered())
                {
                    float wheel = ImGui::GetIO().MouseWheel;
                    if (wheel != 0.0f)
                    {
                        float oldZoom = sZoomLevel;
                        sZoomLevel = std::clamp(sZoomLevel + wheel * 0.1f, 0.1f, 10.0f);

                        // Zoom towards mouse position
                        ImVec2 mousePos = ImGui::GetMousePos();
                        ImVec2 windowPos = ImGui::GetWindowPos();
                        ImVec2 relMouse = { mousePos.x - windowPos.x - availSize.x * 0.5f,
                                            mousePos.y - windowPos.y - availSize.y * 0.5f };

                        float zoomRatio = sZoomLevel / oldZoom;
                        sPanOffset.x = relMouse.x - (relMouse.x - sPanOffset.x) * zoomRatio;
                        sPanOffset.y = relMouse.y - (relMouse.y - sPanOffset.y) * zoomRatio;
                    }

                    // Handle panning with middle mouse or left mouse + drag
                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
                        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift))
                    {
                        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
                        if (delta.x == 0.0f && delta.y == 0.0f)
                        {
                            delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                        }
                        sPanOffset.x += delta.x;
                        sPanOffset.y += delta.y;
                        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
                        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                    }
                }

                // Draw checkerboard background for transparency
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = { p0.x + displayWidth, p0.y + displayHeight };
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                // Simple checkerboard pattern
                const float checkSize = 16.0f;
                ImU32 col1 = IM_COL32(60, 60, 60, 255);
                ImU32 col2 = IM_COL32(40, 40, 40, 255);
                //drawList->PushClipRect(p0, p1, true);
                //for (float y = p0.y; y < p1.y; y += checkSize)
                //{
                //    for (float x = p0.x; x < p1.x; x += checkSize)
                //    {
                //        int cx = static_cast<int>((x - p0.x) / checkSize);
                //        int cy = static_cast<int>((y - p0.y) / checkSize);
                //        ImU32 col = ((cx + cy) % 2 == 0) ? col1 : col2;
                //        drawList->AddRectFilled(
                //            { x, y },
                //            { std::min(x + checkSize, p1.x), std::min(y + checkSize, p1.y) },
                //            col
                //        );
                //    }
                //}
                //drawList->PopClipRect();

                // Draw the texture
                ImGui::Image(texture->GetTextureID(), ImVec2(displayWidth, displayHeight), uv0, uv1);

                // Double-click to close
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    sFullscreenMode = false;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }
            }
            ImGui::EndChild();

            // Help text pinned to the bottom of the window's content area
            ImGui::Separator();
            ImGui::TextDisabled("Arrow Keys/A/D = Navigate | Mouse Wheel = Zoom | Middle Mouse/Shift+Drag = Pan | Y = Flip | R = Reset | ESC/Double-Click = Close");

            ImGui::End();
        }

        bool RenderTextureBrowser(ECS* ecs)
        {
            PROFILE_SCOPE("Texture Browser");

            auto rr = ecs->GetResource<RenderingResource>();
            if (!rr) return false;

            auto tl = rr->textureLibrary.get();
            if (!tl) return false;

            float flipY = static_cast<float>(Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL);

            // Render the viewer window alongside the browser whenever it's open
            if (sFullscreenMode)
            {
                RenderFullscreenViewer(tl, flipY);
            }

            ImVec2 uv0 = { 0.0f, flipY };
            ImVec2 uv1 = { 1.0f, 1.0f - flipY };

            ImGui::Begin("Texture Browser");

            const uint32_t textureCount = tl->GetTextureCount();

            // Search/filter bar
            static char searchBuffer[256] = "";
            ImGui::SetNextItemWidth(200);
            ImGui::InputTextWithHint("##Search", "Search textures...", searchBuffer, sizeof(searchBuffer));
            ImGui::SameLine();
            ImGui::Text("(%d textures)", textureCount);
            ImGui::SameLine();
            ImGui::TextDisabled("(?) Double-click to view");

            ImGui::Separator();

            const float thumbnailSize = 64.0f;
            ImGuiStyle& style = ImGui::GetStyle();
            const float cellSize = thumbnailSize + style.ItemSpacing.x;
            const float panelWidth = ImGui::GetContentRegionAvail().x;
            int columns = (int)floor((panelWidth + style.ItemSpacing.x) / cellSize);
            if (columns <= 0) columns = 1;

            ImGui::BeginChild("TextureGrid", ImVec2(0, 0), false);
            ImGui::Columns(columns, nullptr, false);

            std::string searchStr = searchBuffer;
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

            for (uint32_t i = 0; i < textureCount; ++i)
            {
                ITexture* texture = tl->GetTextureByIndex(i);
                if (!texture) continue;

                // Filter by search
                if (!searchStr.empty())
                {
                    std::string texName = texture->mData.name;
                    std::transform(texName.begin(), texName.end(), texName.begin(), ::tolower);
                    if (texName.find(searchStr) == std::string::npos)
                    {
                        continue;
                    }
                }

                ImGui::PushID(i);

                // Highlight on hover
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                bool hovered = ImGui::IsMouseHoveringRect(cursorPos,
                    ImVec2(cursorPos.x + thumbnailSize, cursorPos.y + thumbnailSize));

                if (hovered)
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(
                        ImVec2(cursorPos.x - 2, cursorPos.y - 2),
                        ImVec2(cursorPos.x + thumbnailSize + 2, cursorPos.y + thumbnailSize + 2),
                        IM_COL32(100, 100, 200, 100),
                        4.0f
                    );
                }

                ImGui::Image(texture->GetTextureID(), ImVec2(thumbnailSize, thumbnailSize), uv0, uv1);

                // Double-click to open viewer
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    sSelectedTextureIndex = static_cast<int32_t>(i);
                    sFullscreenMode = true;
                    sZoomLevel = 1.0f;
                    sPanOffset = { 0.0f, 0.0f };
                }

                // Tooltip on hover
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", texture->mData.name.c_str());
                    ImGui::Text("Size: %dx%d", texture->mData.width, texture->mData.height);
                    ImGui::TextDisabled("Double-click to view");
                    ImGui::EndTooltip();
                }

                ImGui::NextColumn();
                ImGui::PopID();
            }

            ImGui::Columns(1);
            ImGui::EndChild();

            ImGui::End();

            return false;
        }
    }
}