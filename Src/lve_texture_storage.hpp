#pragma once

#include "lve_device.hpp"
#include "lve_descriptors.hpp"

#include <string>
#include <unordered_map>

namespace lve {
	
	class LveTextureStorage {
	public:
		struct TextureData {
			VkImage image;
			VkImageView imageView;
			VkDeviceMemory imageMemory;
			int texWidth; 
			int texHeight;
			std::unordered_map<std::string, VkDescriptorSet> textureDescriptors;
		};

		LveTextureStorage(LveDevice& device);
		~LveTextureStorage();

		LveTextureStorage(const LveTextureStorage&) = delete;
		LveTextureStorage& operator=(const LveTextureStorage&) = delete;

		bool loadTexture(const std::string& texturePath, const std::string& textureName);
		bool loadTexture(const char* image, const int& imageSize, const std::string& textureName);
		void unloadTexture(const std::string& textureName);

		VkSampler createTextureSampler(VkSamplerCreateInfo& samplerInfo, const std::string& samplerName);
		void destroySampler(const std::string& samplerName);

		VkDescriptorImageInfo descriptorInfo(const std::string& samplerName, const std::string& textureName);

		const LveDescriptorSetLayout& getTextureDescriptorSetLayout() const { return *textureSetLayout; }
		const VkDescriptorSet getDescriptorSet(const std::string& textureName, const std::string& samplerName);

		const TextureData& getTextureData (const std::string& textureName);
		bool ContainTexture(const std::string& textureName);

	private:
		void createTextureImage(LveTextureStorage::TextureData& imageData, char* pixels);
		void destroyAndFreeTextureData(const TextureData& data);

		VkSampler getSampler(const std::string& samplerName);

		std::unordered_map<std::string, TextureData> textureDatas;
		std::unordered_map<std::string, VkSampler> textureSamplers;

		LveDevice& lveDevice;
		std::unique_ptr<LveDescriptorPool> texturePool;
		std::unique_ptr<LveDescriptorSetLayout> textureSetLayout;
	};
} // namespace lve