#pragma once

#include "Buffer.h"

namespace Dog
{
    struct AccelerationStructure
    {
        VkAccelerationStructureKHR accel{};
        VkDeviceAddress            address{};
        Buffer                     buffer;    // Underlying buffer
    };
}
