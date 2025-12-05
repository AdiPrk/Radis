#pragma once

#include "AccelerationStructures.h"

namespace Radis
{
	class Device;

	class Allocator {
	public:
		Allocator() = default;
		~Allocator() = default;

		static void Init(Device* device);
		static void Destroy();

		static VmaAllocator GetAllocator() { return mAllocator; }

        // Creates buffer of given size
		static VkResult CreateBuffer(Buffer& buffer,
							  VkDeviceSize size,
							  VkBufferUsageFlags2KHR usage,
							  VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
							  VmaAllocationCreateFlags flags = {},
							  VkDeviceSize minAlignment = 0);

		static VkResult CreateBuffer(Buffer& buffer,
			                  const VkBufferCreateInfo& bufferInfo,
			                  const VmaAllocationCreateInfo& allocInfo,
			                  VkDeviceSize                   minAlignment = 0);

		static void DestroyBuffer(VkBuffer buffer, VmaAllocation bufferAllocation);
		static void DestroyBuffer(Buffer& buffer);

		static VkResult CreateAcceleration(AccelerationStructure& accel, const VkAccelerationStructureCreateInfoKHR& accInfo);
		static VkResult CreateAcceleration(AccelerationStructure& accel,
			const VkAccelerationStructureCreateInfoKHR& accInfo,
			const VmaAllocationCreateInfo& vmaInfo,
			std::span<const uint32_t>                   queueFamilies = {});

		static void DestroyAcceleration(AccelerationStructure& accel);

		/*********************************************************************
	 * param:  imageInfo: Create info for image to create
	 * param:  memoryUsage: Properties of the memory to be created for
	 *                     wanted image
	 * param:  image:      Will be set to created image
	 * param:  imageMemory: Will be set to created image memory
	 *
	 * brief:  Creates an image from passed create info and then
	 *         allocates and bind memory to the image. Both the image
	 *         and the image memory will be set in passed references.
	 *         Throws errors if any creation or allocation failed.
	 *********************************************************************/
		static void CreateImageWithInfo(const VkImageCreateInfo& imageInfo, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& imageAllocation);

		static void SetAllocationName(VmaAllocation allocation, const char* pName);

	private:
		static VmaAllocator mAllocator;
        static Device* mDevice;
	};

}