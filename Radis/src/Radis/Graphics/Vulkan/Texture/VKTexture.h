#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Radis
{
	class Device;

	class VKTexture : public ITexture
	{
	public:
		VKTexture(Device& device, const TextureData& textureData);
		~VKTexture();

        const VkImage& GetImage() const { return mTextureImage; }
		const VkImageView& GetImageView() const { return mTextureImageView; }
        VkFormat GetImageFormat() const { return mData.imageFormat; }
		static void TransitionImageLayout(Device& mDevice, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
		VkExtent2D GetExtent() const { 
			return VkExtent2D{ static_cast<uint32_t>(mData.width), static_cast<uint32_t>(mData.height) };
		}

        void* GetTextureID() override { return reinterpret_cast<void*>(mDescriptorSet); }

		VkDescriptorSet mDescriptorSet;

	private:
		void CreateSpecial();
		void CreateTextureImageCompressed();
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
