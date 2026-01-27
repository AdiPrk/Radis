/*****************************************************************//**
 * \file   PresentSystem.h
 * \brief  Handles presentation to the screen
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../ISystem.h"

namespace Radis
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