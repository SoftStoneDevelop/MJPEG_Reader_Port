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
        void Draw(VkCommandBuffer commandBuffer, LveTextureStorage& lveTextureStorage);
    private:
        struct Camera
        {
            volatile bool stop = false;
            int CameraNumber;
            std::string CameraName;
            std::thread thread;
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
        std::mutex m;
    };
}  // namespace lve