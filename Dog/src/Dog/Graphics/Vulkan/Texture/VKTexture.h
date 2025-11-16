#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Dog
{
	class Device;

	class VKTexture : public ITexture
	{
	public:
		VKTexture(Device& device, const TextureData& textureData);
		~VKTexture();

		const VkImageView& GetImageView() const { return mTextureImageView; }
		static void TransitionImageLayout(Device& mDevice, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

        void* GetTextureID() override { return reinterpret_cast<void*>(mDescriptorSet); }

		VkDescriptorSet mDescriptorSet;

	private:
		void CreateTextureImage();
		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);
		void GenerateMipmaps();
		void CreateTextureImageView();

		VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

		Device& mDevice;

		VkImage mTextureImage;
		VmaAllocation mTextureImageAllocation;
		VkImageView mTextureImageView;

		uint32_t mMipLevels;
		
		friend class TextureLibrary;
	};
}
