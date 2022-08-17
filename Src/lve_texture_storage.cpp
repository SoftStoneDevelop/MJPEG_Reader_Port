#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vulkan/vulkan.h>
#include "lve_texture_storage.hpp"
#include <stdexcept>
#include "lve_buffer.hpp"
#include "Definitions/DefaultSamplersNames.hpp"
#include "lve_swap_chain.hpp"

#ifndef ENGINE_DIR
#define ENGINE_DIR "../../../" 
#endif // !ENGINE_DIR
#include <Helper/VulkanHelpers.hpp>

namespace lve {

    LveTextureStorage::LveTextureStorage(
        LveDevice& device,
        std::shared_ptr<LveRenderer> renderer
    )
        : lveDevice{ device }, lveRenderer{std::move(renderer)}
    {
        texturePool = LveDescriptorPool::Builder(lveDevice)
            .setMaxSets(3000)
            .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
            .build();

        textureSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build()
            ;

        unloadThread = std::thread(&LveTextureStorage::unloadRoutine, this);
    }

    LveTextureStorage::~LveTextureStorage()
    {
        requestDestruct = true;
        {
            for (auto& kv : textureDatas)
            {
                auto& textureData = kv.second;
                destroyAndFreeTextureData(textureData);
            }
        }

        cv.notify_all();
        unloadThread.join();
        while (!unloadQueue.empty())
        {
            auto& item = unloadQueue.front();
            destroyAndFreeTextureData(item);
            unloadQueue.pop();
        }
    }

    VkDescriptorImageInfo LveTextureStorage::descriptorInfo(const std::string& textureName)
    {
        auto& data = getTextureData(textureName);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = data.imageView;
        imageInfo.sampler = data.sampler;

        return imageInfo;
    }

    const LveTextureStorage::TextureData& LveTextureStorage::getTextureData(const std::string& textureName)
    {
        if (textureDatas.count(textureName) == 0)
        {
            throw std::runtime_error("Not find texture with name:" + textureName);
        }
        else
        {
            return textureDatas[textureName];
        }
    }

    VkSampler LveTextureStorage::createTextureSampler(
        VkSamplerCreateInfo& samplerInfo
    )
    {
        VkSampler textureSampler{};
        auto result = vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &textureSampler);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create texture sampler!" + VulkanHelpers::AsString(result));
        }

        return textureSampler;
    }

    void LveTextureStorage::destroySampler(VkSampler sampler)
    {
        vkDestroySampler(lveDevice.device(), sampler, nullptr);
    }

    void LveTextureStorage::createTextureImage(
        LveTextureStorage::TextureData& imageData,
        char* pixels,
        uint32_t mipLevels
    )
    {
        //TODO not always generate Mipmaps, add parametr to method needGenerateMipmap

        VkDeviceSize imageSize = imageData.texWidth * imageData.texHeight * 4;

        if (!pixels) 
        {
            throw std::runtime_error("failed to load image!");
        }

        LveBuffer stagingBuffer{
            lveDevice,
            imageSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)pixels);
        stbi_image_free(pixels);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = imageData.texWidth;
        imageInfo.extent.height = imageData.texHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        lveDevice.createImageWithInfo(
            imageInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            imageData.image,
            imageData.imageMemory
        );

        lveDevice.transitionImageLayout(
            imageData.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipLevels
        );

        lveDevice.copyBufferToImage(
            stagingBuffer.getBuffer(),
            imageData.image,
            static_cast<uint32_t>(imageData.texWidth),
            static_cast<uint32_t>(imageData.texHeight),
            1,
            0//original image
        );

        lveDevice.generateMipmaps(imageData.image, VK_FORMAT_R8G8B8A8_SRGB, imageData.texWidth, imageData.texHeight, mipLevels);
    }

    bool LveTextureStorage::loadTexture(
        const std::string& texturePath,
        VkSamplerCreateInfo& samplerInfo,
        TextureData* data
    ) 
    {
        if (!data)
        {
            return false;
        }

        int texChannels;
        stbi_uc* pixels = stbi_load((ENGINE_DIR + texturePath).c_str(), &data->texWidth, &data->texHeight, &texChannels, STBI_rgb_alpha);
        if (data->texWidth <= 0 || data->texHeight <= 0)
        {
            return false;
        }

        uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(data->texWidth, data->texHeight)))) + 1;
        createTextureImage(*data, reinterpret_cast<char*>(pixels), mipLevels);
        lveDevice.createImageView(
            data->imageView,
            data->image,
            VK_FORMAT_R8G8B8A8_SRGB,
            mipLevels
        );

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);
        samplerInfo.mipLodBias = 0.0f;

        data->sampler = createTextureSampler(samplerInfo);
        return true;
    }

    bool LveTextureStorage::loadTexture(
        const char* image,
        const int& imageSize,
        VkSamplerCreateInfo& samplerInfo,
        TextureData* data
    )
    {
        if (!data)
        {
            return false;
        }

        int texChannels;
        auto imagePtr = reinterpret_cast<const stbi_uc*>(image);
        stbi_uc* pixels = stbi_load_from_memory(imagePtr, imageSize, &data->texWidth, &data->texHeight, &texChannels, STBI_rgb_alpha);
        if (data->texWidth <= 0 || data->texHeight <= 0)
        {
            return false;
        }

        uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(data->texWidth, data->texHeight)))) + 1;
        createTextureImage(*data, reinterpret_cast<char*>(pixels), mipLevels);
        lveDevice.createImageView(
            data->imageView,
            data->image,
            VK_FORMAT_R8G8B8A8_SRGB,
            mipLevels
        );

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);
        samplerInfo.mipLodBias = 0.0f;

        data->sampler = createTextureSampler(samplerInfo);
        return true;
    }

    void LveTextureStorage::storeTexture(const std::string& textureName, TextureData&& data)
    {
        assert(textureDatas.count(textureName) == 0 && "Texture already in use");
        textureDatas[textureName] = std::move(data);
    }

    void LveTextureStorage::changeName(const std::string& textureName, const std::string& newTetureName)
    {
        auto textureData = std::move(textureDatas.at(textureName));
        textureDatas.erase(textureName);
        textureDatas.emplace(newTetureName, textureData);
    }

    void LveTextureStorage::unloadTexture(const std::string& textureName) 
    {
        if (textureDatas.count(textureName) == 0)
            return;

        auto& textureData = textureDatas.at(textureName);
        textureData.unloadAskFrame = lveRenderer->getCurrentFrameCount();
        {
            std::lock_guard lg = std::lock_guard(qM);
            unloadQueue.emplace(std::move(textureData));
        }
        cv.notify_all();

        textureDatas.erase(textureName);
    }

    void LveTextureStorage::destroyAndFreeTextureData(const TextureData& data) 
    {
        for (auto i = data.textureDescriptors.begin(); i != data.textureDescriptors.end(); i++)
        {
            auto& item = *i;
            texturePool->freeDescriptors(&item.second, 1);
        }

        vkDestroySampler(lveDevice.device(), data.sampler, nullptr);
        vkDestroyImageView(lveDevice.device(), data.imageView, nullptr);
        vkDestroyImage(lveDevice.device(), data.image, nullptr);
        vkFreeMemory(lveDevice.device(), data.imageMemory, nullptr);
    }

    const VkDescriptorSet LveTextureStorage::getDescriptorSet(
        const std::string& textureName,
        const std::string& samplerName
    )
    {
        if (textureDatas.count(textureName) == 0)
            return nullptr;

        auto& textureData = textureDatas.at(textureName);
        if (textureData.textureDescriptors.count(samplerName) != 0)
            return textureData.textureDescriptors.at(samplerName);

        auto descriptorImage = descriptorInfo(textureName);
        VkDescriptorSet descriptorSet{};
        LveDescriptorWriter(*textureSetLayout, *texturePool)
            .writeImage(0, &descriptorImage)
            .build(descriptorSet);

        textureData.textureDescriptors[samplerName] = descriptorSet;
        return descriptorSet;
    }

    bool LveTextureStorage::ContainTexture(const std::string& textureName)
    {
        return textureDatas.count(textureName) != 0;
    }

    void LveTextureStorage::unloadRoutine()
    {
        while (true)
        {
            std::unique_lock ul = std::unique_lock(qM);
            cv.wait(ul);
            if (requestDestruct)
            {
                return;
            }

            while (!requestDestruct && !unloadQueue.empty())
            {
                auto currentFrame = lveRenderer->getCurrentFrameCount();
                auto& item = unloadQueue.front();
                if (item.unloadAskFrame < currentFrame - LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                {
                    destroyAndFreeTextureData(item);
                    unloadQueue.pop();
                }
                else
                {
                    using namespace std::literals::chrono_literals;
                    ul.unlock();
                    std::this_thread::sleep_for(1s);
                    ul.lock();
                }
            }

            if (requestDestruct)
            {
                return;
            }
        }
    }

}//namespace lve