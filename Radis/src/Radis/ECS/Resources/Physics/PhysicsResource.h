#pragma once

#include "../IResource.h"

namespace Dog
{
    struct PhysicsResource : public IResource
    {
        PhysicsResource();

    private:
        int a;
    };
}
