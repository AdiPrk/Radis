#pragma once

#include "../ISystem.h"

namespace Dog
{
    class PresentSystem : public ISystem
    {
    public:
        PresentSystem() : ISystem("PresentSystem") {};
        ~PresentSystem() {}

        void Init();
        void FrameStart();
        void Update(float dt);
        void FrameEnd();
        void Exit();
    };
}