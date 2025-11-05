#pragma once

namespace Dog
{
    struct EditorResource;
    class ECS;

    namespace EditorWindows
    {
        void RenderEntitiesWindow(EditorResource* er, ECS* ecs);
    }
}
