#pragma once

#include "IResource.h"

namespace Dog
{
    struct SwapRendererBackendResource : public IResource
    {
        void RequestSwap() { swapRequested = true; }
        bool SwapRequested() { return swapRequested; }

        void RequestVulkan();
        void RequestOpenGL();

    private:
        friend class SwapRendererBackendSystem;
        bool swapRequested = false;
    };
}
