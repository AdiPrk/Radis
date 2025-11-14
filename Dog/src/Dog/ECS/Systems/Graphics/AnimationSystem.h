#pragma once

#include "../ISystem.h"

namespace Dog
{
    class AnimationSystem : public ISystem
    {
    public:
        AnimationSystem() : ISystem("AnimationSystem") {};
        ~AnimationSystem() {}

        void Update(float dt);
    };
}