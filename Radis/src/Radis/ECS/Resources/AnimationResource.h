#pragma once

#include "IResource.h"

namespace Radis
{
    struct AnimationResource : public IResource
    {
        AnimationResource();

        std::vector<VQS> bonesMatrices;
    };
}
