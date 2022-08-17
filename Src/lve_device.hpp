#pragma once

#include "lve_window.hpp"

// std lib headers
#include <string>
#include <vector>
#include <mutex>

namespace lve 
{

struct SwapChainSupportDetails 
{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices 
{
  uint32_t graphicsFamily;
  uint32_t presentFamily;
  bool graphicsFamilyHasValue = false;
  bool presentFamilyHasValue = false;
  bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
};

class LveDevice 
{
 public:
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif

  LveDevice(LveWindow &window);
  ~LveDevice();

  // Not copyable or movable
  LveDevice(const LveDevice &) = delete;
  LveDevice& operator=(const LveDevice &) = delete;
  LveDevice(LveDevice &&) = delete;
  LveDevice &operator=(LveDevice &&) = delete;

  VkDevice device() { return device_; }
  VkInstance getInstance() { return instance; }
  VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
  VkSurfaceKHR surface() { return surface_; }
  std::unique_lock<std::mutex> getLockGraphicsQueue() noexcept { return std::unique_lock<std::mutex>(mGraphicsQueue_, std::defer_lock); };
  VkQueue graphicsQueue() { return graphicsQueue_; }
  VkQueue presentQueue() { return presentQueue_; }

  SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
  QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }
  VkFormat findSupportedFormat(
      const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

  // Buffer Helper Functions
  void createBuffer(
      VkDeviceSize size,
      VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkBuffer &buffer,
      VkDeviceMemory &bufferMemory
  );

  VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);

  void endSingleTimeCommands(
      VkCommandBuffer commandBuffer,
      VkCommandPool commandPool
  );

  void copyBuffer(
      VkBuffer srcBuffer,
      VkBuffer dstBuffer,
      VkDeviceSize size,
      VkCommandPool commandPool
  );

  void copyBufferToImage(
      VkBuffer buffer,
      VkImage image,
      uint32_t width,
      uint32_t height,
      uint32_t layerCount,
      uint32_t mipLevel,
      VkCommandPool commandPool
  );

  void transitionImageLayout(
      VkImage image,
      VkImageLayout oldLayout,
      VkImageLayout newLayout,
      uint32_t mipLevels,
      VkCommandPool commandPool
  );

  void generateMipmaps(
      VkImage image,
      VkFormat imageFormat,
      int32_t texWidth,
      int32_t texHeight,
      uint32_t mipLevels,
      VkCommandPool commandPool
  );

  void createImageWithInfo(
      const VkImageCreateInfo &imageInfo,
      VkMemoryPropertyFlags properties,
      VkImage &image,
      VkDeviceMemory &imageMemory
  );

  void createImageView(
      VkImageView& imageView,
      VkImage& image,
      VkFormat format,
      uint32_t mipLevels = 1
  );

  VkCommandPool createCommandPool();

  VkPhysicalDeviceProperties properties;

 private:
  void createInstance();
  void setupDebugMessenger();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();

  // helper functions
  bool isDeviceSuitable(VkPhysicalDevice device);
  std::vector<const char *> getRequiredExtensions();
  bool checkValidationLayerSupport();
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
  void hasGflwRequiredInstanceExtensions();
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  LveWindow &window;

  VkDevice device_;
  VkSurfaceKHR surface_;
  VkQueue graphicsQueue_;
  std::mutex mGraphicsQueue_;
  VkQueue presentQueue_;

  const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
  const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};

}  // namespace lve