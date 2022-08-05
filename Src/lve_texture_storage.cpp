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
        for (auto& kv : textureDatas)
        {
            auto& textureData = kv.second;
            destroyAndFreeTextureData(textureData);
        }

        cv.notify_all();
        unloadThread.join();
        while (!unloadQueue.empty())
        {
            auto& item = unloadQueue.front();
            destroyAndFreeTextureData(item);
            unloadQueue.pop();
        }

        for (auto& kv : textureSamplers)
        {
            auto& textureSampler = kv.second;
            vkDestroySampler(lveDevice.device(), textureSampler, nullptr);
        }
    }

    VkDescriptorImageInfo LveTextureStorage::descriptorInfo(const std::string& samplerName, const std::string& textureName)
    {
        auto& data = getTextureData(textureName);
        auto sampler = getSampler(samplerName);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = data.imageView;
        imageInfo.sampler = sampler;

        return imageInfo;
    }

    VkSampler LveTextureStorage::getSampler(const std::string& samplerName)
    {
        if (textureSamplers.count(samplerName) == 0)
        {
            if (samplerName == defaultSamplerName)
            {
                VkSamplerCreateInfo defaultSampler{};
                defaultSampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                defaultSampler.magFilter = VK_FILTER_LINEAR;
                defaultSampler.minFilter = VK_FILTER_LINEAR;
                defaultSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                defaultSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                defaultSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                defaultSampler.anisotropyEnable = VK_TRUE;
                defaultSampler.maxAnisotropy = lveDevice.properties.limits.maxSamplerAnisotropy;
                defaultSampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                defaultSampler.unnormalizedCoordinates = VK_FALSE;
                defaultSampler.compareEnable = VK_FALSE;
                defaultSampler.compareOp = VK_COMPARE_OP_ALWAYS;
                defaultSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

                auto textureSampler = createTextureSampler(defaultSampler, samplerName);

                return textureSampler;
            }

            return nullptr;
        }
        else
        {
            return textureSamplers[samplerName];
        }
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
        VkSamplerCreateInfo& samplerInfo,
        const std::string& samplerName
    )
    {
        VkSampler textureSampler{};
        if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }

        assert(textureSamplers.count(samplerName) == 0 && "Sampler already in use");
        textureSamplers[samplerName] = textureSampler;

        return textureSampler;
    }

    void LveTextureStorage::destroySampler(const std::string& samplerName)
    {
        if (textureSamplers.count(samplerName) == 0)
            return;

        auto& textureSampler = textureSamplers.at(samplerName);
        vkDestroySampler(lveDevice.device(), textureSampler, nullptr);
        textureDatas.erase(samplerName);
    }

    void LveTextureStorage::createTextureImage(LveTextureStorage::TextureData& imageData, char* pixels)
    {
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
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        lveDevice.copyBufferToImage(
            stagingBuffer.getBuffer(),
            imageData.image,
            static_cast<uint32_t>(imageData.texWidth),
            static_cast<uint32_t>(imageData.texHeight),
            1
        );

        lveDevice.transitionImageLayout(
            imageData.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    bool LveTextureStorage::loadTexture(const std::string& texturePath, const std::string& textureName) {
        LveTextureStorage::TextureData imageData{};
        int texChannels;
        stbi_uc* pixels = stbi_load((ENGINE_DIR + texturePath).c_str(), &imageData.texWidth, &imageData.texHeight, &texChannels, STBI_rgb_alpha);
        if (imageData.texWidth <= 0 || imageData.texHeight <= 0)
        {
            return false;
        }

        createTextureImage(imageData, reinterpret_cast<char*>(pixels));
        lveDevice.createImageView(imageData.imageView, imageData.image, VK_FORMAT_R8G8B8A8_SRGB);

        assert(textureDatas.count(textureName) == 0 && "Texture already in use");
        textureDatas[textureName] = std::move(imageData);
        return true;
    }

    bool LveTextureStorage::loadTexture(const char* image, const int& imageSize, const std::string& textureName) 
    {
        LveTextureStorage::TextureData imageData{};
        int texChannels;
        auto imagePtr = reinterpret_cast<const stbi_uc*>(image);
        stbi_uc* pixels = stbi_load_from_memory(imagePtr, imageSize, &imageData.texWidth, &imageData.texHeight, &texChannels, STBI_rgb_alpha);
        if (imageData.texWidth <= 0 || imageData.texHeight <= 0)
        {
            return false;
        }
        createTextureImage(imageData, reinterpret_cast<char*>(pixels));
        lveDevice.createImageView(imageData.imageView, imageData.image, VK_FORMAT_R8G8B8A8_SRGB);

        assert(textureDatas.count(textureName) == 0 && "Texture already in use");
        textureDatas[textureName] = std::move(imageData);
        return true;
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

        auto descriptorImage = descriptorInfo(samplerName, textureName);
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
        }
    }

}//namespace lve