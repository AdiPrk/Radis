/*****************************************************************//**
 * \file   AnimationResource.cpp
 * \brief  Animation resource for ECS
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "AnimationResource.h"

namespace Radis
{
    AnimationResource::AnimationResource()
    {
        bonesMatrices.reserve(10000);
    }
}
