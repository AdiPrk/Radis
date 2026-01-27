/*****************************************************************//**
 * \file   AnimationSystem.h
 * \brief  Handles animating entities
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "../ISystem.h"

namespace Radis
{
    class AnimationSystem : public ISystem
    {
    public:
        AnimationSystem() : ISystem("AnimationSystem") {};
        ~AnimationSystem() {}

        void Update(float dt);
    };
}