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
    // Convert GLM mat4 to Vulkan transform matrix for acceleration structures
    VkTransformMatrixKHR Radis::ToTransformMatrixKHR(const glm::mat4& m)
    {
        VkTransformMatrixKHR t;
        glm::mat4 tmp = glm::transpose(m);
        memcpy(&t, glm::value_ptr(tmp), sizeof(t));
        return t;
    }

    // Generate a Halton sequence value for a given index and base
    float Halton(uint32_t index, uint32_t base) {
        float f = 1.0f;
        float r = 0.0f;
        while (index > 0) {
            f = f / static_cast<float>(base);
            r = r + f * static_cast<float>(index % base);
            index = index / base;
        }
        return r;
    }

}