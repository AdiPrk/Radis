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
