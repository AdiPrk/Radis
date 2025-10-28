#pragma once

#include "ISystem.h"


namespace Dog
{
    class SwapRendererBackendSystem : public ISystem
    {
    public:
        SwapRendererBackendSystem() : ISystem("SwapRendererBackendSystem") {}
        ~SwapRendererBackendSystem() {}

        void FrameStart();
    };
}

