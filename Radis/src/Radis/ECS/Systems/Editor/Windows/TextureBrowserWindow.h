#pragma once

namespace Radis
{
    class ECS;
    class TextureLibrary;

    namespace EditorWindows
    {
        void RenderFullscreenViewer(TextureLibrary* tl, float flipY);
        bool RenderTextureBrowser(ECS* ecs);
    }
}
