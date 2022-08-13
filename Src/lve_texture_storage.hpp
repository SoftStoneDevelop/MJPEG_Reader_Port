#pragma once

#include "lve_device.hpp"
#include "lve_descriptors.hpp"

#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include "lve_renderer.hpp"

namespace lve 
{
	class LveTextureStorage 
	{
	public:
		struct TextureData 
		{
			VkImage image;
			VkImageView imageView;
			VkDeviceMemory imageMemory;
			int texWidth; 
			int texHeight;
			std::unordered_map<std::string, VkDescriptorSet> textureDescriptors;
			int unloadAskFrame;

			VkSampler sampler;
		};

		LveTextureStorage(LveDevice& device, std::shared_ptr<LveRenderer> renderer);
		~LveTextureStorage();

		LveTextureStorage(const LveTextureStorage&) = delete;
		LveTextureStorage& operator=(const LveTextureStorage&) = delete;

		bool loadTexture(
			const std::string& texturePath,
			const std::string& textureName,
			VkSamplerCreateInfo& samplerInfo
		);
		bool loadTexture(
			const char* image,
			const int& imageSize,
			const std::string& textureName,
			VkSamplerCreateInfo& samplerInfo
		);
		void unloadTexture(const std::string& textureName);

		VkSampler createTextureSampler(VkSamplerCreateInfo& samplerInfo);
		void destroySampler(VkSampler sampler);

		VkDescriptorImageInfo descriptorInfo(const std::string& textureName);

		const LveDescriptorSetLayout& getTextureDescriptorSetLayout() const { return *textureSetLayout; }
		const VkDescriptorSet getDescriptorSet(const std::string& textureName, const std::string& samplerName);

		const TextureData& getTextureData (const std::string& textureName);
		bool ContainTexture(const std::string& textureName);
	private:
		void createTextureImage(
			LveTextureStorage::TextureData& imageData,
			char* pixels,
			uint32_t mipLevels
		);
		void destroyAndFreeTextureData(const TextureData& data);
		void unloadRoutine();

		std::unordered_map<std::string, TextureData> textureDatas;

		LveDevice& lveDevice;
		std::unique_ptr<LveDescriptorPool> texturePool;
		std::unique_ptr<LveDescriptorSetLayout> textureSetLayout;

		std::shared_ptr<LveRenderer> lveRenderer;

		std::thread unloadThread;
		std::queue<TextureData> unloadQueue;
		std::mutex qM;
		std::condition_variable cv;
		volatile bool requestDestruct = false;
	};

} // namespace lve