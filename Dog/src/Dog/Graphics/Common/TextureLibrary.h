#pragma once

#include "Graphics/RHI/ITexture.h"

namespace Dog
{
    class Device;
	class ITexture;
	class Uniform;

	class TextureLibrary
	{
	public:
		TextureLibrary(Device* device);
		~TextureLibrary();

		uint32_t AddTexture(const std::string& texturePath);
		uint32_t AddTexture(const unsigned char* textureData, uint32_t textureSize, const std::string& texturePath);
		uint32_t CreateImage(const std::string& imageName, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageUsageFlags usage, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL);

		ITexture* GetTexture(uint32_t textureID);
		ITexture* GetTexture(const std::string& texturePath);
		ITexture* GetTextureByIndex(uint32_t index);

		uint32_t GetTextureCount() const { return static_cast<uint32_t>(mTextures.size()); }
        VkSampler GetSampler() const { return mTextureSampler; }

		const static uint32_t MAX_TEXTURE_COUNT;
		const static uint32_t INVALID_TEXTURE_INDEX;

		void ClearAllBuffers(class Device* device);
		void RecreateAllBuffers(class Device* device);

		void CreateTextureSampler();
		void CreateDescriptors();
		void CreateDescriptorSet(class VKTexture* texture, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        void SetDevice(Device* dev) { device = dev; }

	private:

		friend class VKTexture;
		std::vector<std::unique_ptr<ITexture>> mTextures;
		std::vector<TextureData> mTexturesData;
		std::unordered_map<std::string, uint32_t> mTextureMap;

		Device* device;
		VkSampler mTextureSampler;
		VkDescriptorSetLayout mImageDescriptorSetLayout;
		VkDescriptorPool mImageDescriptorPool;
	};

} // namespace Dog
