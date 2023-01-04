#pragma once

#include "lve_device.hpp"
#include "lve_window.hpp"
#include "lve_descriptors.hpp"
#include "lve_texture_storage.hpp"

#include "../ArrayPool/Src/ArrayPool.hpp"
#include "../ThreadPool/Src/ThreadPool.hpp"

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
        struct IncomeData
        {
            char* data;
            int dataSize;
        };

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
            struct ImageData
            {
                char* pixels;
                int texWidth;
                int texHeight;
            };

            std::unique_lock<std::mutex> GetLockQueuePixel() { return std::unique_lock<std::mutex>(queueM); }
            void PushData(char* pixels, int texWidth, int texHeight)
            {
                unprocessedPixels.emplace(pixels, texWidth, texHeight);
            };

            void ResetPixel(char* pixels)
            {
                if (pixels)
                {
                    stbi_image_free(pixels);
                }
            };

            volatile bool stop = false;
            int CameraNumber;
            std::string CameraName;
            std::thread thread;

            std::queue<ImageData> unprocessedPixels;
        private:
            std::mutex queueM;
        };

        void readImageStream(
            std::shared_ptr<Camera> camera,
            std::string host,
            std::string path,
            std::string port
        );

        void convertPixel(
            std::shared_ptr<Camera> camera,
            std::queue<IncomeData>* imageQueue,
            std::mutex* imageQueueM,
            std::condition_variable* cv
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
        //ThreadPool::ThreadPool threadPool{0};

        std::atomic<int> cameraIndex = 0;
        int selectedCamera;
    };
}  // namespace lve