#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Dog
{
	class Device;

	class VKTexture : public ITexture
	{
	public:
		// From file/data
		VKTexture(Device& device, const std::string& path, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB);
		VKTexture(Device& device, const std::string& name, const unsigned char* textureData, uint32_t textureSize, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB);
		VKTexture(Device& device, const UncompressedPixelData& data, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB);

		// Storage texture
		VKTexture(Device& device, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageUsageFlags usage, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL);

		~VKTexture();

		const int GetWidth() const { return mWidth; }
		const int GetHeight() const { return mHeight; }
		const VkImageView& GetImageView() const { return mTextureImageView; }
		static void TransitionImageLayout(Device& mDevice, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

        void* GetTextureID() override { return reinterpret_cast<void*>(mDescriptorSet); }

		VkDescriptorSet mDescriptorSet;

	private:

		void LoadPixelsFromFile(const std::string& filepath);
		void LoadPixelsFromMemory(const unsigned char* textureData, int textureSize);

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
		
		unsigned char* mPixels;

		friend class TextureLibrary;
		VkFormat mImageFormat;		
		VkImageUsageFlags mUsage;
		VkImageLayout mFinalLayout;
	};
}
