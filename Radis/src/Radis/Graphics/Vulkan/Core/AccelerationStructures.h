/*****************************************************************//**
 * \file   AccelerationStructures.h
 * \brief  Definition of the AccelerationStructure struct for Vulkan ray tracing.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "Buffer.h"

namespace Radis
{
    struct AccelerationStructure
    {
        VkAccelerationStructureKHR accel{};
        VkDeviceAddress            address{};
        Buffer                     buffer;    // Underlying buffer
        uint32_t                   instanceCount;
    };
}
