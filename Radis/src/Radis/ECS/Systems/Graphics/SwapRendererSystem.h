#pragma once

#include "../ISystem.h"


namespace Radis
{
    class SwapRendererSystem : public ISystem
    {
    public:
        SwapRendererSystem() : ISystem("SwapRendererSystem") {}
        ~SwapRendererSystem() {}

        void Init() override;
        void FrameStart() override;
        void FrameEnd() override;
    };
}