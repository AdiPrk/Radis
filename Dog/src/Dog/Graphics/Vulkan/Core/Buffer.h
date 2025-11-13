#pragma once

namespace Dog
{
    struct Buffer
    {
        VkBuffer        buffer{};
        VkDeviceSize    bufferSize{};
        VkDeviceAddress address{};
        uint8_t*        mapping{};
        VmaAllocation   allocation{};
    };
}