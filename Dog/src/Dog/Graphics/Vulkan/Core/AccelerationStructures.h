#pragma once

#include "Buffer.h"

namespace Dog
{
    struct ABuffer
    {
        VkBuffer        buffer{};
        VkDeviceSize    bufferSize{};
        VkDeviceAddress address{};
        uint8_t* mapping{};
        VmaAllocation   allocation{};
    };

    struct AccelerationStructure
    {
        VkAccelerationStructureKHR accel{};
        VkDeviceAddress            address{};
        ABuffer                    buffer;    // Underlying buffer
    };
}
