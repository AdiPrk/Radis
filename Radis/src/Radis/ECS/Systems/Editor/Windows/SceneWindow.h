#pragma once

namespace Radis
{
    class ECS;

    namespace EditorWindows
    {
        void RenderSceneWindow(ECS* ecs);
        void UpdateImGuizmo(ECS* ecs);
    }
}
