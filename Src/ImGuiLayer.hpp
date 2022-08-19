#pragma once

#include "lve_device.hpp"
#include "lve_window.hpp"
#include "lve_descriptors.hpp"
#include "lve_texture_storage.hpp"

#include <ArrayPool.hpp>

// libs
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <stb_image.h>

namespace lve 
{
    class ImGuiLayer
    {
    public:
        ImGuiLayer(
            LveWindow& window,
            LveDevice& device,
            VkRenderPass renderPass,
            uint32_t imageCount,
            LveTextureStorage& lveTextureStorage,
            VkCommandPool commandPool
        );
        ~ImGuiLayer();
        void Draw(
            VkCommandBuffer commandBuffer,
            LveTextureStorage& lveTextureStorage,
            VkCommandPool commandPool
        );
    private:
        struct Camera
        {
        public:
            char* GetPixelPtr() const { return unprocessedPixels; }
            std::unique_lock<std::mutex> GetLockPixel() { return std::unique_lock<std::mutex>(pixelsM); }
            void SetPixel(char* pixels)
            {
                if (unprocessedPixels)
                {
                    stbi_image_free(unprocessedPixels);
                }
                unprocessedPixels = pixels;
            };

            void ResetPixel()
            {
                if (unprocessedPixels)
                {
                    stbi_image_free(unprocessedPixels);
                }
                unprocessedPixels = nullptr;
            };

            volatile bool stop = false;
            int CameraNumber;
            std::string CameraName;
            std::thread thread;

            int texWidth;
            int texHeight;
        private:
            char* unprocessedPixels = nullptr;
            std::mutex pixelsM;
        };

        void readImageStream(
            std::shared_ptr<Camera> camera,
            std::string host,
            std::string path,
            std::string port
        );
        bool validatePort(const char* input);
        bool validateHost(const char* input);

        bool portValide = false;
        bool hostValide = false;

        LveDevice& device;
        LveTextureStorage& lveTextureStorage;

        std::unique_ptr<LveDescriptorPool> imGuiPool{};
        ImGuiWindowFlags flags;

        char host[16];
        char port[5];
        char* path;
        int pathSize;
        std::queue<std::shared_ptr<Camera>> cameras;
        ArrayPool::ArrayPool<char> pool;

        std::atomic<int> cameraIndex = 0;
        int selectedCamera;
    };
}  // namespace lve