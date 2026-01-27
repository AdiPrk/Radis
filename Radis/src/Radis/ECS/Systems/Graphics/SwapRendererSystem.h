/*****************************************************************//**
 * \file   SwapRendererSystem.h
 * \brief  Used to swap between different rendering backends
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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