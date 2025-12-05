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