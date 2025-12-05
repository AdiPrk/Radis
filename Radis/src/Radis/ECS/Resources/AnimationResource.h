#pragma once

#include "IResource.h"

namespace Dog
{
    struct AnimationResource : public IResource
    {
        AnimationResource();

        std::vector<VQS> bonesMatrices;
    };
}
