#pragma once

#include "../ISystem.h"


namespace Dog
{
    class SwapRendererBackendSystem : public ISystem
    {
    public:
        SwapRendererBackendSystem() : ISystem("SwapRendererBackendSystem") {}
        ~SwapRendererBackendSystem() {}

        void Init() override;
        void FrameStart() override;

        void SwapBackend();
    };
}

