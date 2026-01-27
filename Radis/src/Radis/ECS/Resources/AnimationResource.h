/*****************************************************************//**
 * \file   AnimationResource.h
 * \brief  Resource for storing animation data
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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
