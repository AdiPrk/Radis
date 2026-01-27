/*****************************************************************//**
 * \file   VKMath.h
 * \brief  Declaration of Vulkan-related math utility functions.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    VkTransformMatrixKHR toTransformMatrixKHR(const glm::mat4& m);
}
