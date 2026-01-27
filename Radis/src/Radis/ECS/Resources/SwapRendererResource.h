/*****************************************************************//**
 * \file   SwapRendererResource.h
 * \brief  Handles swapping between graphical backends
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "IResource.h"

namespace Radis
{
    struct SwapRendererResource : public IResource
    {
        void RequestSwap() { swapRequested = true; }
        bool SwapRequested() { return swapRequested; }

        void RequestVulkan();
        void RequestOpenGL();

    private:
        friend class Engine;

        // Careful with this!
        void SwapBackend(class ECS* ecs, bool isAtInitializaton = false);

        friend class SwapRendererSystem;
        bool swapRequested = false;
    };
}
