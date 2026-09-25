/*****************************************************************//**
 * \file   AnimationResource.h
 * \brief  Resource for storing animation data
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "IResource.h"
#include "Graphics/Common/Animation/Skeleton.h"

namespace Radis
{
    struct AnimationResource : public IResource
    {
        AnimationResource();

        std::vector<SkinMatrix> skinMatrices; // every animated entity's palette, back to back
    };
}
