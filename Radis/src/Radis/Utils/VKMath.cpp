/*****************************************************************//**
 * \file   VKMath.cpp
 * \brief  Implementation of Vulkan-related math utility functions.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "VKMath.h"

namespace Radis
{

    VkTransformMatrixKHR Radis::ToTransformMatrixKHR(const glm::mat4& m)
    {
        VkTransformMatrixKHR t;
        glm::mat4 tmp = glm::transpose(m);
        memcpy(&t, glm::value_ptr(tmp), sizeof(t));
        return t;
    }

}