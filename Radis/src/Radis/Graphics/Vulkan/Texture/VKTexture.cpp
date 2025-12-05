#include <PCH/pch.h>
#include "VKTexture.h"

#include "../Core/Device.h"
#include "../Core/Buffer.h"

#include "stb_image.h"

namespace Dog
{
	VKTexture::VKTexture(Device& device, const TextureData& textureData)
		: ITexture(textureData)
		, mDevice(device)
	{
		if (textureData.isStorageImage)
		{
			mMipLevels = 1; // Storage images don't have mipmaps

			// Create the image
			CreateImage(mData.width, mData.height, mData.imageFormat, textureData.tiling, textureData.usage, VMA_MEMORY_USAGE_GPU_ONLY);
			TransitionImageLayout(mDevice, mTextureImage, mData.imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, textureData.finalLayout, mMipLevels);
			CreateTextureImageView();
		}
		else if (textureData.isSpecialImage)
		{
			CreateSpecial();
		}
		else if (mData.isCompressed)
		{
			mMipLevels = mData.mipLevels;
			CreateTextureImageCompressed();
			CreateTextureImageView();
		}
		else 
		{
			mMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(mData.width, mData.height)))) + 1;

			CreateTextureImage();
			CreateTextureImageView();
		}
	}

	VKTexture::~VKTexture()
	{
		if (mTextureImageView)
		{
			vkDestroyImageView(mDevice.GetDevice(), mTextureImageView, nullptr);
			mTextureImageView = VK_NULL_HANDLE;
		}

		if (mTextureImage)
		{
			vmaDestroyImage(Allocator::GetAllocator(), mTextureImage, mTextureImageAllocation);
			mTextureImage = VK_NULL_HANDLE;
			mTextureImageAllocation = VK_NULL_HANDLE;
		}
	}

	void VKTexture::CreateSpecial()
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = mData.imageFormat;
		imageInfo.extent.width = mData.width;
		imageInfo.extent.height = mData.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = mData.tiling;
		imageInfo.usage = mData.usage;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		VkResult result = vmaCreateImage(Allocator::GetAllocator(), &imageInfo, &allocInfo, &mTextureImage, &mTextureImageAllocation, nullptr);
		if (result != VK_SUCCESS)
		{
			DOG_CRITICAL("VMA failed to create special image");
		}
        Allocator::SetAllocationName(mTextureImageAllocation, "Special Texture Image");

		VkImageAspectFlags aspect = 0;

		if (mData.imageFormat == VK_FORMAT_D16_UNORM ||
			mData.imageFormat == VK_FORMAT_X8_D24_UNORM_PACK32 ||
			mData.imageFormat == VK_FORMAT_D32_SFLOAT)
		{
			aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else if (mData.imageFormat == VK_FORMAT_S8_UINT)
		{
			aspect = VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else if (mData.imageFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
			mData.imageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
		{
			aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else
		{
			aspect = VK_IMAGE_ASPECT_COLOR_BIT; // regular color textures
		}

		VkImageViewCreateInfo sampledViewInfo{};
		sampledViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		sampledViewInfo.image = mTextureImage;
		sampledViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		sampledViewInfo.format = mData.imageFormat;
		sampledViewInfo.subresourceRange.aspectMask = aspect;
		sampledViewInfo.subresourceRange.baseMipLevel = 0;
		sampledViewInfo.subresourceRange.levelCount = 1;
		sampledViewInfo.subresourceRange.baseArrayLayer = 0;
		sampledViewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(mDevice, &sampledViewInfo, nullptr, &mTextureImageView) != VK_SUCCESS)
		{
			DOG_CRITICAL("Failed to create special image view");
		}
	}

	void VKTexture::CreateTextureImageCompressed()
	{
		// 1. Staging buffer for all mip data
		Buffer stagingBuffer{};
		Allocator::CreateBuffer(
			stagingBuffer,
			mData.pixels.size(),
			VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR,
			VMA_MEMORY_USAGE_AUTO,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			VMA_ALLOCATION_CREATE_MAPPED_BIT
		);
		Allocator::SetAllocationName(stagingBuffer.allocation, "Texture Staging Buffer (Compressed)");

		if (!stagingBuffer.mapping)
		{
			DOG_ERROR("No Mapping found for staging buffer when creating compressed texture image!");
			return;
		}

		std::memcpy(stagingBuffer.mapping, mData.pixels.data(), mData.pixels.size());

		// 2. Create image with compressed format & mips
		VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		CreateImage(
			mData.width,
			mData.height,
			mData.imageFormat,         // VK_FORMAT_BC7_SRGB_BLOCK
			VK_IMAGE_TILING_OPTIMAL,
			usage,
			VMA_MEMORY_USAGE_GPU_ONLY
		);

		// 3. Record transitions + copy in a single command buffer
		VkCommandBuffer cmd = mDevice.BeginSingleTimeCommands();

		// 3a. UNDEFINED -> TRANSFER_DST_OPTIMAL for all mips
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = mTextureImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mMipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		// 3b. Copy each mip from buffer to image
		std::vector<VkBufferImageCopy> regions;
		regions.reserve(mMipLevels);

		for (uint32_t level = 0; level < mMipLevels; ++level)
		{
			const auto& mip = mData.mipInfos[level];

			VkBufferImageCopy region{};
			region.bufferOffset = mip.offset;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = level;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;

			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { mip.width, mip.height, 1 };

			regions.push_back(region);
		}

		vkCmdCopyBufferToImage(
			cmd,
			stagingBuffer.buffer,
			mTextureImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(regions.size()),
			regions.data()
		);

		// 3c. TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL for all mips
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		// 4. Submit + wait
		mDevice.EndSingleTimeCommands(cmd);

		// 5. Destroy staging buffer
		vmaDestroyBuffer(Allocator::GetAllocator(), stagingBuffer.buffer, stagingBuffer.allocation);
	}

	void VKTexture::CreateTextureImage()
	{
		// Create a staging buffer to load texture data
		Buffer stagingBuffer{};
		Allocator::CreateBuffer(
			stagingBuffer,
            mData.pixels.size(),
			VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR,  // Note: new flag type (VkBufferUsageFlags2KHR)
			VMA_MEMORY_USAGE_AUTO,                   // auto-select memory type
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT // ensures host-visible, coherent memory
		);
        Allocator::SetAllocationName(stagingBuffer.allocation, "Texture Staging Buffer");

		// Copy pixel data to staging buffer (mapping is stored in ABuffer.mapping)
		if (stagingBuffer.mapping)
		{
			memcpy(stagingBuffer.mapping, mData.pixels.data(), static_cast<size_t>(mData.pixels.size()));
		}
		else
		{
            DOG_ERROR("No Mapping found for staging buffer when creating texture image!");
			return;
		}

		//Set the usage flags for the image
		VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// Create the Vulkan image
		CreateImage(mData.width, mData.height, mData.imageFormat, VK_IMAGE_TILING_OPTIMAL, usage, VMA_MEMORY_USAGE_GPU_ONLY);
			
		// Transition the image to a transfer destination layout
		TransitionImageLayout(mDevice, mTextureImage, mData.imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mMipLevels);

		// Copy the staging buffer to the image
		CopyBufferToImage(stagingBuffer.buffer, mTextureImage, static_cast<uint32_t>(mData.width), static_cast<uint32_t>(mData.height), 1);

		// Destroy the staging buffer
		vmaDestroyBuffer(Allocator::GetAllocator(), stagingBuffer.buffer, stagingBuffer.allocation);

		// Generate mipmaps
		GenerateMipmaps();
	}

	void VKTexture::TransitionImageLayout(Device& mDevice, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
	{
		VkCommandBuffer commandBuffer = mDevice.BeginSingleTimeCommands(); // Helper to start recording commands

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = 0; // No need to wait for anything in VK_IMAGE_LAYOUT_UNDEFINED
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Shader will read from the image

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Assuming the image is read in the fragment shader
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // We will write to it from the shader

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR; // Use the ray tracing stage
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else {
			throw std::invalid_argument("Unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		mDevice.EndSingleTimeCommands(commandBuffer); // Helper to finish recording and submit the commands
	}

	void VKTexture::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage)
	{
		// The parameters for the image creation
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; // Type of structure
		imageInfo.imageType = VK_IMAGE_TYPE_2D;                // Type of image, could be 1D and 3D too
		imageInfo.extent.width = width;       // Width of the image
		imageInfo.extent.height = height;     // Height of the image
		imageInfo.extent.depth = 1;           // Depth of the image (how many texels on the z axis, which is 1 for 2D)
		imageInfo.mipLevels = mMipLevels;      // Number of mip levels
		imageInfo.arrayLayers = 1;	          // Number of layers in image array (?)
		imageInfo.format = format;            // Format of the image data
		imageInfo.tiling = tiling;            // How texels are laid out in memory
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Layout of the image data on creation
		imageInfo.usage = usage;                             // Bit field of what the image will be used for
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;   // It'll only be accessed by the graphics queue family
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;           // Number of samples for multisampling
		imageInfo.flags = 0; // Optional

		// Setup allocation info for VMA
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = memoryUsage;

		// Use VMA to create image and allocate memory in one step
		if (vmaCreateImage(Allocator::GetAllocator(), &imageInfo, &allocInfo, &mTextureImage, &mTextureImageAllocation, nullptr) != VK_SUCCESS)
		{
            DOG_CRITICAL("Failed to create and allocate image using VMA!");
		}

        std::string dbgName = "Tex_" + mData.name;
        Allocator::SetAllocationName(mTextureImageAllocation, dbgName.c_str());
	}

	void VKTexture::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount)
	{
		VkCommandBuffer commandBuffer = mDevice.BeginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = layerCount;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);

		mDevice.EndSingleTimeCommands(commandBuffer);
	}

	void VKTexture::GenerateMipmaps()
	{
		// Check if image format supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(mDevice.GetPhysicalDevice(), mData.imageFormat, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		{
            DOG_CRITICAL("texture image format does not support linear blitting!");
		}

		VkCommandBuffer commandBuffer = mDevice.BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = mTextureImage;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = mData.width;
		int32_t mipHeight = mData.height;

		for (uint32_t i = 1; i < mMipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				mTextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mMipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		mDevice.EndSingleTimeCommands(commandBuffer);
	}

	void VKTexture::CreateTextureImageView() 
	{
		mTextureImageView = CreateImageView(mTextureImage, mData.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	VkImageView VKTexture::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mMipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(mDevice.GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
		{
            DOG_CRITICAL("Failed to create texture image view!");
		}

		return imageView;
	}
}
