#include <PCH/pch.h>
#include "TextureBrowserWindow.h"

#include "Graphics/Common/TextureLibrary.h"
#include "Graphics/OpenGL/GLFrameBuffer.h"

#include "ECS/Resources/RenderingResource.h"
#include "ECS/Resources/EditorResource.h"
#include "ECS/Components/Components.h"
#include "Engine.h"

namespace Dog
{
    namespace EditorWindows
    {
        void RenderTextureBrowser(ECS* ecs)
        {
            auto rr = ecs->GetResource<RenderingResource>();
            if (!rr) return;

            auto tl = rr->textureLibrary.get();
            if (!tl) return;

            float flipY = static_cast<float>(Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL);
            ImVec2 uv0 = { 0.0f, 1.0f };
            ImVec2 uv1 = { 1.0f, 0.0f };

            ImGui::Begin("Texture Browser");

            const uint32_t textureCount = tl->GetTextureCount();
            const float thumbnailSize = 64.0f;
            const float padding = 8.0f;
            const float cellSize = thumbnailSize + padding;
            const float panelWidth = ImGui::GetContentRegionAvail().x;
            const int columns = std::max(static_cast<int>(panelWidth / cellSize), 1);
            ImGui::Columns(columns, nullptr, false);
            for (uint32_t i = 0; i < textureCount; ++i)
            {
                ITexture* texture = tl->GetTextureByIndex(i);
                if (!texture) continue;

                ImGui::PushID(i);
                ImGui::Image(reinterpret_cast<ImTextureID>(texture->GetTextureID()), ImVec2(thumbnailSize, thumbnailSize), uv0, uv1);
                ImGui::NextColumn();
                ImGui::PopID();
            }

            ImGui::End();
        }
    }
}
