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

    // Generate a Halton sequence value for a given index and base
    float Halton(uint32_t index, uint32_t base);

    // Divides and rounds up
    static inline uint32_t DivUp(uint32_t a, uint32_t b) { return (a + b - 1) / b; }
}
