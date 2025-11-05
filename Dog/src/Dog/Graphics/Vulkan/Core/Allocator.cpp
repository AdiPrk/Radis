#include <PCH/pch.h>
#include "Allocator.h"

#include "../Core/Device.h"

namespace Dog {

    Allocator::Allocator(Device& device)
        : mDevice(device)
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = device.GetPhysicalDevice();
        allocatorInfo.device = device.GetDevice();
        allocatorInfo.instance = device.GetInstance();
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;  // allow using VkBufferUsageFlags2CreateInfoKHR

        vmaCreateAllocator(&allocatorInfo, &mAllocator);
    }

    Allocator::~Allocator()
    {
        vmaDestroyAllocator(mAllocator);
    }

    void Allocator::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBuffer& buffer, VmaAllocation& bufferAllocation, VkDeviceSize minAlignment)
    {
        // Buffer info
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Allocation info
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;
        allocInfo.flags = 0;

        VkResult result = vmaCreateBufferWithAlignment(mAllocator, &bufferInfo, &allocInfo, minAlignment, &buffer, &bufferAllocation, nullptr);

        if (result != VK_SUCCESS)
        {
            const char* errorStr = "";
            switch (result)
            {
                case VK_ERROR_OUT_OF_HOST_MEMORY: errorStr = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
                case VK_ERROR_OUT_OF_DEVICE_MEMORY: errorStr = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
                case VK_ERROR_INITIALIZATION_FAILED: errorStr = "VK_ERROR_INITIALIZATION_FAILED"; break;
                case VK_ERROR_MEMORY_MAP_FAILED: errorStr = "VK_ERROR_MEMORY_MAP_FAILED"; break;
                case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: errorStr = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"; break;
                default: errorStr = "Unknown VkResult error"; break;
            }

            printf("Failed to create buffer using VMA!\n");
            printf("  VkResult: %d (%s)\n", result, errorStr);
            printf("  Size: %llu bytes\n", static_cast<unsigned long long>(size));
            printf("  Usage: 0x%X\n", usage);
            printf("  MemoryUsage: %d\n", memoryUsage);
            printf("  MinAlignment: %llu\n", static_cast<unsigned long long>(minAlignment));
        }

        // get bda
        // const VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
        // VkDeviceAddress address = vkGetBufferDeviceAddress(mDevice.GetDevice(), &info);

        // if (address == 0)
        // {
        //     DOG_ERROR("Failed to get buffer device address!");
        // }
    }

    VkResult Allocator::CreateBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags2KHR usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags, VkDeviceSize minAlignment)
    {
        const VkBufferUsageFlags2CreateInfo bufferUsageFlags2CreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
            .usage = usage,
        };

        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = &bufferUsageFlags2CreateInfo,
            .size = size,
            .usage = 0,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VmaAllocationCreateInfo allocInfo = { .flags = flags, .usage = memoryUsage };

        return CreateBuffer(buffer, bufferInfo, allocInfo, minAlignment);
    }

    VkResult Allocator::CreateBuffer(Buffer& buffer, const VkBufferCreateInfo& bufferInfo, const VmaAllocationCreateInfo& allocInfo, VkDeviceSize minAlignment) const
    {
        buffer = {};

        // Create the buffer
        VmaAllocationInfo allocInfoOut{};
            
        VkResult result = vmaCreateBufferWithAlignment(mAllocator, &bufferInfo, &allocInfo, minAlignment,
            &buffer.buffer, &buffer.allocation, &allocInfoOut);

        if (result != VK_SUCCESS)
        {
            // Handle allocation failure
            DOG_CRITICAL("Failed to create buffer");
            return result;
        }

        buffer.bufferSize = bufferInfo.size;
        buffer.mapping = static_cast<uint8_t*>(allocInfoOut.pMappedData);

        // Get the GPU address of the buffer
        const VkBufferUsageFlags2KHR usage = reinterpret_cast<const VkBufferUsageFlags2CreateInfo*>(bufferInfo.pNext)->usage;

        if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            const VkBufferDeviceAddressInfo addrInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = buffer.buffer
            };
            buffer.address = vkGetBufferDeviceAddress(mDevice.GetDevice(), &addrInfo);
        }
        else
        {
            buffer.address = 0;
        }

        return result;
    }

    void Allocator::DestroyBuffer(VkBuffer buffer, VmaAllocation bufferAllocation)
    {
        vmaDestroyBuffer(mAllocator, buffer, bufferAllocation);
        buffer = VK_NULL_HANDLE;
        bufferAllocation = VK_NULL_HANDLE;
    }

    void Allocator::DestroyBuffer(Buffer& buffer) const
    {
        vmaDestroyBuffer(mAllocator, buffer.buffer, buffer.allocation);
        buffer = {};
    }

    VkResult Allocator::CreateAcceleration(AccelerationStructure& accel, const VkAccelerationStructureCreateInfoKHR& accInfo) const
    {
        const VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };

        return CreateAcceleration(accel, accInfo, allocInfo);
    }

    VkResult Allocator::CreateAcceleration(AccelerationStructure& accel, const VkAccelerationStructureCreateInfoKHR& accInfo, const VmaAllocationCreateInfo& vmaInfo, std::span<const uint32_t> queueFamilies) const
    {
        accel = {};
        VkAccelerationStructureCreateInfoKHR accelStruct = accInfo;

        const VkBufferUsageFlags2CreateInfo bufferUsageFlags2CreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT,
        };

        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = &bufferUsageFlags2CreateInfo,
            .size = accelStruct.size,
            .usage = 0,
            .sharingMode = queueFamilies.empty() ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
            .pQueueFamilyIndices = queueFamilies.data(),
        };

        // Step 1: Create the buffer to hold the acceleration structure
        VkResult result = CreateBuffer(accel.buffer, bufferInfo, vmaInfo);

        if (result != VK_SUCCESS)
        {
            return result;
        }

        // Step 2: Create the acceleration structure with the buffer
        accelStruct.buffer = accel.buffer.buffer;
        result = mDevice.g_vkCreateAccelerationStructureKHR(mDevice.GetDevice(), &accelStruct, nullptr, &accel.accel);

        if (result != VK_SUCCESS)
        {
            DestroyBuffer(accel.buffer);
            DOG_ERROR("Failed to create acceleration structure");
            return result;
        }

        {
            VkAccelerationStructureDeviceAddressInfoKHR info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                                                             .accelerationStructure = accel.accel };
            accel.address = mDevice.g_vkGetAccelerationStructureDeviceAddressKHR(mDevice.GetDevice(), &info);
        }

        return result;
    }

    void Allocator::DestroyAcceleration(AccelerationStructure& accel) const
    {
        DestroyBuffer(accel.buffer);
        mDevice.g_vkDestroyAccelerationStructureKHR(mDevice, accel.accel, nullptr);
        accel = {};
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