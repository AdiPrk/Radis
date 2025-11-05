#pragma once

namespace Dog
{
    struct RenderingResource;
    struct EditorResource;
    class ECS;

    namespace EditorWindows
    {
        void RenderSceneWindow(ECS* ecs);
        void UpdateImGuizmo(ECS* ecs);
    }
}
