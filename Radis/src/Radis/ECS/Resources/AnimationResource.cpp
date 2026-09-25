/*****************************************************************//**
 * \file   AnimationResource.cpp
 * \brief  Animation resource for ECS
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "AnimationResource.h"
#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

namespace Radis
{
    AnimationResource::AnimationResource()
    {
        skinMatrices.reserve(AnimationUniforms::MAX_BONES);
    }
}
