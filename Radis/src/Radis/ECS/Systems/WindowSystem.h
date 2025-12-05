#pragma once

#include "ISystem.h"

namespace Radis
{
    class WindowSystem : public ISystem
    {
    public:
        WindowSystem() : ISystem("WindowSystem") {};
        ~WindowSystem() {}

        void Update(float dt) override;
        void FrameEnd() override;
    };
}