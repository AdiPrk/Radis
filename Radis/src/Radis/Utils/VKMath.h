/*****************************************************************//**
 * \file   VKMath.h
 * \brief  Vulkan-specific math conversion utilities
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    // Convert GLM mat4 to Vulkan transform matrix for acceleration structures
    VkTransformMatrixKHR ToTransformMatrixKHR(const glm::mat4& m);
}
