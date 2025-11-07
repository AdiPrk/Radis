#pragma once

namespace Dog
{
    class ECS;

    namespace EditorWindows
    {
        void RenderSceneWindow(ECS* ecs);
        void UpdateImGuizmo(ECS* ecs);
    }
}
