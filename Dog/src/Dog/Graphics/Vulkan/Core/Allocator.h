#pragma once

#include "AccelerationStructures.h"

namespace Dog
{
	class Device;

	class Allocator {
	public:
		Allocator(Device& device);
		~Allocator();

		VmaAllocator GetAllocator() { return mAllocator; }

        // Creates buffer of given size
		void CreateBuffer(
			VkDeviceSize size,
			VkBufferUsageFlags usage,
			VmaMemoryUsage memoryUsage,
			VkBuffer& buffer,
			VmaAllocation& bufferAllocation,
			VkDeviceSize minAlignment = 0);

		VkResult CreateBuffer(
			ABuffer& buffer,
			VkDeviceSize size,
			VkBufferUsageFlags2KHR usage,
			VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
			VmaAllocationCreateFlags flags = {},
			VkDeviceSize minAlignment = 0);

		VkResult CreateBuffer(ABuffer& buffer,
			                  const VkBufferCreateInfo& bufferInfo,
			                  const VmaAllocationCreateInfo& allocInfo,
			                  VkDeviceSize                   minAlignment = 0) const;

		void DestroyBuffer(VkBuffer buffer, VmaAllocation bufferAllocation);
		void DestroyBuffer(ABuffer& buffer) const;

		VkResult CreateAcceleration(AccelerationStructure& accel, const VkAccelerationStructureCreateInfoKHR& accInfo) const;
		VkResult CreateAcceleration(AccelerationStructure& accel,
			const VkAccelerationStructureCreateInfoKHR& accInfo,
			const VmaAllocationCreateInfo& vmaInfo,
			std::span<const uint32_t>                   queueFamilies = {}) const;

		void DestroyAcceleration(AccelerationStructure& accel) const;

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
		void CreateImageWithInfo(const VkImageCreateInfo& imageInfo, VmaMemoryUsage memoryUsage, VkImage& image, VmaAllocation& imageAllocation);

	private:
		VmaAllocator mAllocator;
        Device& mDevice;
	};

}