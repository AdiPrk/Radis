/*****************************************************************//**
 * \file   Buffer.h
 * \brief  Defines a structure for managing Vulkan buffers with VMA allocation.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
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