/*****************************************************************//**
 * \file   WindowSystem.h
 * \brief  Swaps the window buffer
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "ISystem.h"

namespace Radis
{
    class WindowSystem : public ISystem
    {
    public:
        WindowSystem() : ISystem("WindowSystem") {};
        ~WindowSystem() {}

        void FrameEnd() override;
    };
}