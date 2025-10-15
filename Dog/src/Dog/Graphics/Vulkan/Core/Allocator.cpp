#include <PCH/pch.h>
#include "Allocator.h"

#include "../Core/Device.h"

namespace Dog {

    Allocator::Allocator(Device& device)
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = device.GetPhysicalDevice();
        allocatorInfo.device = device.GetDevice();
        allocatorInfo.instance = device.GetInstance();
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        vmaCreateAllocator(&allocatorInfo, &mAllocator);
    }

    Allocator::~Allocator()
    {
        vmaDestroyAllocator(mAllocator);
    }

    void Allocator::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBuffer& buffer, VmaAllocation& bufferAllocation)
    {
        // Buffer info
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Allocation info
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;
        allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        // Create the buffer and allocate memory using VMA
        if (vmaCreateBuffer(mAllocator, &bufferInfo, &allocInfo, &buffer, &bufferAllocation, nullptr) != VK_SUCCESS)
        {
            DOG_CRITICAL("failed to create buffer using VMA!");
        }
    }

    void Allocator::DestroyBuffer(VkBuffer buffer, VmaAllocation bufferAllocation)
    {
        vmaDestroyBuffer(mAllocator, buffer, bufferAllocation);
    }

    void Allocator::CreateImageWithInfo(
        const VkImageCreateInfo& imageInfo,
        VmaMemoryUsage memoryUsage,
        VkImage& image,
        VmaAllocation& imageAllocation)
    {
        // Allocation info for VMA
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        // Create the image and allocate memory using VMA
        if (vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &image, &imageAllocation, nullptr) != VK_SUCCESS)
        {
            DOG_CRITICAL("failed to create image with VMA!");
        }
    }
}