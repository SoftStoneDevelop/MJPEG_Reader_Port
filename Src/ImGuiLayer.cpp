#pragma once

#include "ImGuiLayer.hpp"
#include "HttpClient.hpp"

#include "Helper/ArrayExt.hpp"
#include <iostream>
#include "Definitions/DefaultSamplersNames.hpp"
#include <fstream>
#include <format>
#include "Helper/VulkanHelpers.hpp"
#include <imgui_internal.h>
#include "Helper/VulkanExtensions.hpp"

namespace lve
{
	ImGuiLayer::ImGuiLayer(
		LveWindow& window,
		LveDevice& lveDevice,
		VkRenderPass renderPass,
		uint32_t imageCount,
        LveTextureStorage& textureStorage,
        VkCommandPool commandPool
    ) 
        : device{ lveDevice },
        lveTextureStorage{ textureStorage },
        path{ new char[100] },
        pathSize{100}
	{
        imGuiPool = LveDescriptorPool::Builder(device)
            .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Initialize imgui for vulkan
        ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = device.getInstance();
        init_info.PhysicalDevice = device.getPhysicalDevice();
        init_info.Device = device.device();
        init_info.QueueFamily = device.findPhysicalQueueFamilies().graphicsFamily;
        init_info.Queue = device.graphicsQueue();

        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = imGuiPool->getDescriptorPool();
        init_info.Allocator = VK_NULL_HANDLE;
        init_info.MinImageCount = 2;
        init_info.ImageCount = imageCount;
        init_info.CheckVkResultFn = nullptr;
        ImGui_ImplVulkan_Init(&init_info, renderPass);

        auto commandBuffer = device.beginSingleTimeCommands(commandPool);
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        device.endSingleTimeCommands(commandBuffer, commandPool);
        ImGui_ImplVulkan_DestroyFontUploadObjects();

        flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        flags &= ~ImGuiWindowFlags_NoScrollbar;

        memset(host, 0, sizeof(host));
        memset(port, 0, sizeof(port));
        memset(path, 0, sizeof(path));

        //test data
        {
            const char* temp = "/mjpg/video.mjpg";
            int i = 0;
            while (temp[i] != '\0')
            {
                path[i] = temp[i];
                i++;
            }

            path[i] = '\0';

            temp = "31.160.161.51";
            i = 0;
            while (temp[i] != '\0')
            {
                host[i] = temp[i];
                i++;
            }

            host[i] = '\0';
            hostValide = validateHost(port);

            temp = "8081";
            i = 0;
            while (temp[i] != '\0')
            {
                port[i] = temp[i];
                i++;
            }

            port[i] = '\0';
            portValide = validatePort(port);
        }
	}

	ImGuiLayer::~ImGuiLayer()
	{
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        int size = cameras.size();
        while (size > 0)
        {
            auto item = std::move(cameras.front());
            cameras.pop();
            item->stop = true;
            cameras.push(std::move(item));
            size--;
        }

        while (!cameras.empty())
        {
            auto item = std::move(cameras.front());
            cameras.pop();
            if (item->thread.joinable())
            {
                item->thread.join();
            }
        }

        delete[] path;
	}

    void ImGuiLayer::Draw(
        VkCommandBuffer commandBuffer,
        LveTextureStorage& lveTextureStorage,
        VkCommandPool commandPool
    )
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto showWindow = true;
        auto tempValid = false;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);

        if (ImGui::Begin("Example: Fullscreen window", &showWindow, flags))
        {
            {
                if (!hostValide)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                }
                ImGui::PushItemWidth(150);

                auto hostChange = ImGui::InputText("##Host", host, sizeof(host));
                if (hostChange)
                {
                    tempValid = validateHost(host);
                }
                ImGui::PopItemWidth();
                if (!hostValide)
                {
                    ImGui::PopStyleColor();
                }

                if (hostChange)
                {
                    hostValide = tempValid;
                }

                ImGui::SameLine();
                ImGui::Text("Host");
            }
            ImGui::SameLine();
            {
                if (!portValide)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                }

                ImGui::PushItemWidth(50);
                auto portChange = ImGui::InputText("##Port", port, sizeof(port));
                if (portChange)
                {
                    tempValid = validatePort(port);
                }

                if (!portValide)
                {
                    ImGui::PopStyleColor();
                }

                if (portChange)
                {
                    portValide = tempValid;
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::Text("Port");
            }
            ImGui::SameLine();
            {
                ImGui::PushItemWidth(200);
                auto pathChange = ImGui::InputText("##Path", path, pathSize);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::Text("Path");
            }
            ImGui::SameLine();
            {
                if (!hostValide || !portValide)
                {
                    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                }
                if (ImGui::Button("Add camera"))
                {
                    auto camera = std::make_shared<Camera>();
                    camera->CameraName = "Some name";
                    camera->CameraNumber = cameraIndex++;
                    camera->thread = std::thread(
                        &ImGuiLayer::readImageStream,
                        this,
                        camera,
                        std::string(host),
                        std::string(path),
                        std::string(port)
                    );

                    cameras.push(std::move(camera));
                }
                if (!hostValide || !portValide)
                {
                    ImGui::PopItemFlag();
                    ImGui::PopStyleVar();
                }
            }
            {
                auto frameText =
                    std::format(
                        "Application average {:.3f}f ms/frame ({:.1f}f FPS)",
                        1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate
                    );

                auto textSize = ImGui::CalcTextSize(frameText.c_str());
                ImGui::BeginListBox(
                    "Cameras",
                    ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y - textSize.y)
                );
                ImGui::PushID("##VerticalScrolling");

                {//camera items
                    int qSize = cameras.size();
                    while (qSize > 0)
                    {
                        auto item = std::move( cameras.front());
                        cameras.pop();
                        auto textureName = std::format("camera{}", item->CameraNumber);

                        auto lock = item->GetLockQueuePixel();

                        if (!item->unprocessedPixels.empty())
                        {
                            auto& imageData = item->unprocessedPixels.front();
                            item->unprocessedPixels.pop();
                            lock.unlock();
                            VkSamplerCreateInfo sampler{};
                            sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                            sampler.magFilter = VK_FILTER_LINEAR;
                            sampler.minFilter = VK_FILTER_LINEAR;
                            sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                            sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                            sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                            sampler.anisotropyEnable = VK_TRUE;
                            sampler.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy;
                            sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                            sampler.unnormalizedCoordinates = VK_FALSE;
                            sampler.compareEnable = VK_FALSE;
                            sampler.compareOp = VK_COMPARE_OP_ALWAYS;

                            LveTextureStorage::TextureData textureData{};
                            textureData.texHeight = imageData.texHeight;
                            textureData.texWidth = imageData.texWidth;
                            if (lveTextureStorage.loadTexture(imageData.pixels, sampler, &textureData, commandPool))
                            {
                                lveTextureStorage.unloadTexture(textureName);
                                lveTextureStorage.storeTexture(textureName, std::move(textureData));
                            }

                            item->ResetPixel(imageData.pixels);
                        }
                        else
                        {
                            lock.unlock();
                        }

                        if (lveTextureStorage.ContainTexture(textureName))
                        {
                            auto& tData = lveTextureStorage.getTextureData(textureName);
                            auto info = lveTextureStorage.getDescriptorSet(textureName);
                            auto itemSizeAv = ImGui::GetContentRegionAvail();
                            ImGui::Image(
                                info,
                                {
                                    //TODO Calculate ratio
                                    itemSizeAv.x >= (float)tData.texWidth ? (float)tData.texWidth : itemSizeAv.x,
                                    (float)tData.texHeight
                                }
                            );
                        }

                        ImGui::PushID((void*)item.get());
                        if (ImGui::Button("Close camera"))
                        {
                            item->stop = true;
                            if (item->thread.joinable())
                            {
                                item->thread.join();
                            }
                        }
                        else
                        {
                            cameras.push(std::move(item));
                        }
                        ImGui::PopID();
                        qSize--;

                    }//camera items
                }

                ImGui::PopID();
                ImGui::EndListBox();
                ImGui::Text(frameText.c_str());
            }

            ImGui::End();
        }

        ImGui::Render();
        auto drawdata = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawdata, commandBuffer);
    }

    void ImGuiLayer::readImageStream(
        std::shared_ptr<Camera> camera,
        std::string host,
        std::string path,
        std::string port
    )
    {
        ClientMJPEG::HttpClient client(
            host,
            port
        );
        std::string error;
        client.Connect(&error);
        client.SendRequestGetOnStream(std::move(path), &error);

        const char* boundaryMark = "boundary=";
        auto textureName = std::format("camera{}", camera->CameraNumber);

        int readBufferSize = 4000;
        int realSize;
        char* readBuffer = pool.Rent(readBufferSize, realSize);
        readBufferSize = realSize;

        std::future<int> readAsync = client.ReadAsync(readBuffer, readBufferSize);

        int indxBoundary = 0;
        auto foundMark = false;
        while (!camera->stop)
        {
            auto readSize = readAsync.get();
            if (readSize <= 0)
            {
                break;
            }

            int index = 0;
            for (; index < readSize; index++)
            {
                if (readBuffer[index] == boundaryMark[indxBoundary])
                {
                    if (indxBoundary == 8)//boundaryMarkSize - 1
                    {
                        index++;
                        foundMark = true;
                    }

                    indxBoundary++;
                }
                else
                {
                    indxBoundary = 0;
                }

                if (foundMark)
                {
                    break;
                }
            }

            if (foundMark)
            {
                if (index == readSize - 1)
                {
                    readAsync = client.ReadAsync(readBuffer, readBufferSize);
                }
                else
                {
                    std::shift_left(readBuffer, readBuffer + readBufferSize, index);
                    readAsync = client.ReadAsync(readBuffer + (readSize - index), readBufferSize - (readSize - index));
                }

                break;
            }

            readAsync = client.ReadAsync(readBuffer, readBufferSize);
        }

        char boundary[70]{};
        int boundarySize = 0;
        int payloadSize = 0;
        while (!camera->stop)
        {
            auto readSize = readAsync.get();
            if (readSize <= 0)
            {
                break;
            }

            auto boundaryEnd = false;
            for (int i = 0; i < readSize; i++)
            {
                if (readBuffer[i] == '"')
                {
                    continue;
                }

                if (readBuffer[i] == '\r')
                {
                    boundaryEnd = true;
                    if (i == readSize - 1)
                    {
                        readAsync = client.ReadAsync(readBuffer, readBufferSize);
                    }
                    else
                    {
                        std::shift_left(readBuffer + i, readBuffer + readSize, readBufferSize - i);
                        readAsync = client.ReadAsync(readBuffer + (readSize - i), readBufferSize - (readSize - i));
                        payloadSize = readSize - i;
                    }

                    break;
                }

                boundary[boundarySize++] = readBuffer[i];
            }

            if (boundaryEnd)
            {
                break;
            }

            readAsync = client.ReadAsync(readBuffer, readBufferSize);
        }

        int imageBufferSize = readBufferSize;
        char* imageBuffer = pool.Rent(imageBufferSize, realSize);
        imageBufferSize = realSize;
        int payloadOffset = 0;

        auto imagesQueue = std::queue<IncomeData>();
        auto imagesQueueM = std::mutex();
        auto cv = std::condition_variable();
        auto convertThread = std::thread(
            &ImGuiLayer::convertPixel,
            this,
            camera,
            &imagesQueue,
            &imagesQueueM,
            &cv
        );

        const char* startDataMark = "\r\n\r\n";

        while (!camera->stop)
        {
            auto readSize = readAsync.get();
            if (readSize <= 0)
            {
                break;
            }

            if (imageBufferSize - payloadSize < readBufferSize)
            {
                char* newImageBuffer = pool.Rent(imageBufferSize * 2, realSize);
                std::copy(imageBuffer, imageBuffer + payloadSize, newImageBuffer);
                pool.Return(imageBuffer);
                imageBuffer = newImageBuffer;
                imageBufferSize = realSize;
            }

            if (imageBufferSize - payloadOffset - payloadSize < readBufferSize)
            {
                std::shift_left(imageBuffer, imageBuffer + payloadOffset + payloadSize, imageBufferSize - payloadSize);
                payloadOffset = 0;
            }

            std::copy(readBuffer, readBuffer + readSize, imageBuffer + payloadOffset + payloadSize);
            payloadSize += readSize;

            readAsync = client.ReadAsync(readBuffer, readBufferSize);

            char* processStart = imageBuffer + payloadOffset;
            int processSize = payloadSize;
            auto boundaryIndex =
                ArrayExt::FindBytesIndex<char>(
                    processStart,
                    processSize,
                    boundary,
                    boundarySize
                    );
            if (boundaryIndex == -1)
            {
                continue;
            }

            processSize = processSize - boundaryIndex - boundarySize;
            auto startData =
                ArrayExt::FindBytesIndex<char>(
                    processStart + boundaryIndex + boundarySize,
                    processSize - boundaryIndex - boundarySize,
                    startDataMark,
                    4
                    );
            if (startData == -1)
            {
                continue;
            }

            startData += boundaryIndex + boundarySize + 4;
            if (startData > processSize)
            {
                continue;
            }

            //next boundary is the end of prev
            auto nextBoundaryIndex =
                ArrayExt::FindBytesIndex<char>(
                    processStart + startData,
                    processSize - startData,
                    boundary,
                    boundarySize
                    );

            if (nextBoundaryIndex == -1)
            {
                continue;
            }

            if (nextBoundaryIndex == 0)
            {
                //no image data
                payloadSize -= startData;
                payloadOffset += startData;
                continue;
            }

            auto incomeData = pool.Rent(nextBoundaryIndex, realSize);
            std::copy(processStart + startData, processStart + startData + nextBoundaryIndex, incomeData);
            {
                std::lock_guard lg(imagesQueueM);
                imagesQueue.emplace(incomeData, nextBoundaryIndex);
                cv.notify_all();
            }

            payloadSize -= startData + nextBoundaryIndex;
            payloadOffset += startData + nextBoundaryIndex;
        }

        if (convertThread.joinable())
        {
            cv.notify_all();
            convertThread.join();
        }

        if (lveTextureStorage.ContainTexture(textureName))
        {
            lveTextureStorage.unloadTexture(textureName);
        }

        while (!imagesQueue.empty())
        {
            pool.Return(imagesQueue.front().data);
            imagesQueue.pop();
        }

        while (!camera->unprocessedPixels.empty())
        {
            camera->ResetPixel(camera->unprocessedPixels.front().pixels);
            camera->unprocessedPixels.pop();
        }

        pool.Return(imageBuffer);
        pool.Return(readBuffer);
        pool.Return(boundary);
    }

    void ImGuiLayer::convertPixel(
        std::shared_ptr<Camera> camera,
        std::queue<IncomeData>* imageQueue,
        std::mutex* imageQueueM,
        std::condition_variable* cv
    )
    {
        while (!camera->stop)
        {
            std::unique_lock ul(*imageQueueM);
            if (imageQueue->empty())
            {
                cv->wait(ul);
                if (camera->stop)
                {
                    break;
                }
            }
            else
            {
                auto image = imageQueue->front();
                imageQueue->pop();
                ul.unlock();

                int imageWidth, imageHeight;
                auto pixels =
                    lveTextureStorage.convertToPixels(
                        image.data,
                        image.dataSize,
                        imageWidth,
                        imageHeight
                    );

                if (pixels)
                {
                    auto cameraLock = camera->GetLockQueuePixel();
                    camera->PushData(pixels, imageWidth, imageHeight);
                    cameraLock.unlock();
                }
                else
                {
                    std::cout << "Image fail convert to pixels, image size:" << image.dataSize << std::endl;
                }

                pool.Return(image.data);
            }
        }
    }

    bool ImGuiLayer::validateHost(const char* input)
    {
        int position = 0;
        char numbers[10] = { '0','1','2','3','4','5','6','7','8','9' };
        int partIndex = 0;

        int dotCounter = 0;
        while (input[position] != '\0')
        {
            if (partIndex == 1 && input[position - 1] == '0' && input[position] != '.')
            {
                return false;
            }

            if (input[position] == '.')
            {
                if (dotCounter > 2)
                {
                    return false;
                }

                dotCounter++;
                if (partIndex > 0)
                {
                    position++;
                    partIndex = 0;
                    continue;
                }
                else
                {
                    return false;
                }
            }

            if (partIndex > 2)
            {
                return false;
            }

            auto number = std::find(numbers, numbers + sizeof(numbers), input[position]);
            position++;
            partIndex++;
            if (number != numbers + sizeof(numbers))
            {
                continue;
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    bool ImGuiLayer::validatePort(const char* input)
    {
        int position = 0;
        char numbers[10] = { '0','1','2','3','4','5','6','7','8','9' };

        if (input[position] == '0')
        {
            return false;
        }

        while (input[position] != '\0')
        {
            auto number = std::find(numbers, numbers + sizeof(numbers), input[position]);
            position++;
            if (number != numbers + sizeof(numbers))
            {
                continue;
            }
            else
            {
                return false;
            }
        }

        return true;
    }
}