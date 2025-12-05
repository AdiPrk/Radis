#include <PCH/pch.h>
#include "Allocator.h"

#include "../Core/device.h"

namespace Radis 
{
    Device* Allocator::mDevice = nullptr;
    VmaAllocator Allocator::mAllocator = nullptr;

    void Allocator::Init(Device* device)
    {
        mDevice = device;

        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = mDevice->GetPhysicalDevice();
        allocatorInfo.device = mDevice->GetDevice();
        allocatorInfo.instance = mDevice->GetInstance();
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;  // allow using VkBufferUsageFlags2CreateInfoKHR

        vmaCreateAllocator(&allocatorInfo, &mAllocator);
    }

    void Allocator::Destroy()
    {
        vmaDestroyAllocator(mAllocator);
        mAllocator = nullptr;
        mDevice = nullptr;
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

    VkResult Allocator::CreateBuffer(Buffer& buffer, const VkBufferCreateInfo& bufferInfo, const VmaAllocationCreateInfo& allocInfo, VkDeviceSize minAlignment)
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
            buffer.address = vkGetBufferDeviceAddress(mDevice->GetDevice(), &addrInfo);
        }
        else
        {
            buffer.address = 0;
        }

        return result;
    }

    void Allocator::DestroyBuffer(VkBuffer buffer, VmaAllocation bufferAllocation)
    {
        if (!buffer || !bufferAllocation)
        {
            return;
        }

        vmaDestroyBuffer(mAllocator, buffer, bufferAllocation);
        buffer = VK_NULL_HANDLE;
        bufferAllocation = VK_NULL_HANDLE;
    }

    void Allocator::DestroyBuffer(Buffer& buffer)
    {
        if (!buffer.buffer || !buffer.allocation)
        {
            return;
        }

        vmaDestroyBuffer(mAllocator, buffer.buffer, buffer.allocation);
        buffer = {};
    }

    VkResult Allocator::CreateAcceleration(AccelerationStructure& accel, const VkAccelerationStructureCreateInfoKHR& accInfo)
    {
        const VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };

        return CreateAcceleration(accel, accInfo, allocInfo);
    }

    VkResult Allocator::CreateAcceleration(AccelerationStructure& accel, const VkAccelerationStructureCreateInfoKHR& accInfo, const VmaAllocationCreateInfo& vmaInfo, std::span<const uint32_t> queueFamilies)
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
        result = vkCreateAccelerationStructureKHR(mDevice->GetDevice(), &accelStruct, nullptr, &accel.accel);

        if (result != VK_SUCCESS)
        {
            DestroyBuffer(accel.buffer);
            DOG_ERROR("Failed to create acceleration structure");
            return result;
        }

        {
            VkAccelerationStructureDeviceAddressInfoKHR info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                                                             .accelerationStructure = accel.accel };
            accel.address = vkGetAccelerationStructureDeviceAddressKHR(mDevice->GetDevice(), &info);
        }

        return result;
    }

    void Allocator::DestroyAcceleration(AccelerationStructure& accel)
    {
        DestroyBuffer(accel.buffer);
        vkDestroyAccelerationStructureKHR(mDevice->GetDevice(), accel.accel, nullptr);
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

    void Allocator::SetAllocationName(VmaAllocation allocation, const char* pName)
    {
        if (mAllocator && allocation && pName)
        {
            vmaSetAllocationName(mAllocator, allocation, pName);
        }
    }
}