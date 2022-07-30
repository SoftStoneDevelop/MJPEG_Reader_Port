#pragma once

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H = true
#include "first_app.hpp"
#include "lve_buffer.hpp"

#include "imgui.hpp"
#include "HttpClient.hpp"
#include <ArrayPool.hpp>
#include "Helper/ArrayExt.hpp"
#include <iostream>

namespace lve {

	FirstApp::FirstApp() {
		imGuiPool = LveDescriptorPool::Builder(lveDevice)
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

		InitializeImGui(lveWindow, lveDevice, lveRenderer.getSwapChainRenderPass(), imGuiPool->getDescriptorPool(), LveSwapChain::MAX_FRAMES_IN_FLIGHT);
	}

	FirstApp::~FirstApp() 
	{
        _stop = true;
        readCameraThread.join();
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void FirstApp::readImageStream()
	{
        ClientMJPEG::HttpClient client("31.160.161.51", 8081);
        std::string error;
        client.Connect(&error);
        client.SendRequestGetOnStream("/mjpg/video.mjpg", &error);

        auto pool = ArrayPool::ArrayPool<char>();
        std::string contentLengthBytes = "\nContent-Length: ";
        std::string newLineBytes = "\n";
        std::string carriageReturnSize = "\r";

        int readBufferSize = 1024;
        int realSize;
        char* readBuffer = pool.Rent(readBufferSize, realSize);
        readBufferSize = realSize;

        int imageBufferSize = readBufferSize;
        char* imageBuffer = pool.Rent(imageBufferSize, realSize);
        imageBufferSize = realSize;
        int newIBufferSize = -1;
        int payloadSize = 0;
        int payloadOffset = 0;

        char* lengthImageBuffer = pool.Rent(sizeof(int), realSize);

        std::future<int> readAsync = client.ReadAsync(readBuffer, readBufferSize);
        while (!_stop)
        {
            auto readSize = readAsync.get();
            if (readSize <= 0)
            {
                break;
            }

            if (newIBufferSize != -1)
            {
                char* newImageBuffer = pool.Rent(newIBufferSize, realSize);
                std::copy(imageBuffer, imageBuffer + payloadSize, newImageBuffer);
                payloadOffset = 0;
                pool.Return(imageBuffer);
                imageBuffer = newImageBuffer;
                imageBufferSize = realSize;

                char* newReadBuffer = pool.Rent(newIBufferSize / 2, realSize);
                std::copy(readBuffer, readBuffer + readBufferSize, newReadBuffer);
                pool.Return(readBuffer);
                readBuffer = newReadBuffer;
                readBufferSize = realSize;

                newIBufferSize = -1;
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

            int processOffset = 0;
            bool process = true;
            while (process)
            {
                if (payloadSize - payloadOffset <= 0)
                {
                    process = false;
                    continue;
                }

                int currentIndex = 0;
                char* processStart = imageBuffer + payloadOffset;
                int processSize = payloadSize;

                currentIndex =
                    ArrayExt::FindBytesIndex<char>(
                        processStart,
                        processSize,
                        contentLengthBytes.c_str(),
                        contentLengthBytes.size()
                        );

                processSize -= currentIndex;
                if (currentIndex == -1)
                {
                    process = false;
                    continue;
                }

                currentIndex += contentLengthBytes.size();
                processSize -= contentLengthBytes.size();
                if (currentIndex > payloadSize)
                {
                    process = false;
                    continue;
                }

                auto endNewLine =
                    ArrayExt::FindBytesIndex<char>(
                        processStart + currentIndex,
                        processSize,
                        newLineBytes.c_str(),
                        newLineBytes.size()
                        );
                if (endNewLine == -1)
                {
                    process = false;
                    continue;
                }

                auto imageSize = std::strtol(processStart + currentIndex, nullptr, 10);
                currentIndex += endNewLine;
                processSize -= endNewLine;
                if (imageSize * 2 > imageBufferSize)
                {
                    newIBufferSize = imageSize * 2;
                }

                if (processSize <= newLineBytes.size() * 2 + carriageReturnSize.size())
                {
                    process = false;
                    continue;
                }
                else
                {
                    currentIndex += newLineBytes.size() * 2 + carriageReturnSize.size();
                    processSize -= newLineBytes.size() * 2 + carriageReturnSize.size();
                }

                if (processSize < imageSize)
                {
                    process = false;
                    continue;
                }

                if (imageSize == processSize)
                {
                    payloadSize = 0;
                    payloadOffset = 0;
                    process = false;
                }
                else
                {
                    payloadOffset += imageSize + currentIndex;
                    payloadSize -= imageSize + currentIndex;
                }

                auto guard = std::lock_guard(_m);
                if (_image == nullptr)
                {
                    _image = pool.Rent(imageSize, realSize);
                    _imageSize = imageSize;
                    _arrSize = realSize;
                    std::copy(processStart + currentIndex, processStart + currentIndex + imageSize, _image);
                }
                else if (_arrSize < imageSize)
                {
                    pool.Return(_image);
                    _image = pool.Rent(imageSize, realSize);
                    _imageSize = imageSize;
                    _arrSize = realSize;
                    std::copy(processStart + currentIndex, processStart + currentIndex + imageSize, _image);
                }
                else
                {
                    _imageSize = imageSize;
                    std::copy(processStart + currentIndex, processStart + currentIndex + imageSize, _image);
                }

                std::cout << "Image with size:" << imageSize << std::endl;
                std::cout << std::endl;
            }
        }

        pool.Return(imageBuffer);
        pool.Return(readBuffer);
        pool.Return(lengthImageBuffer);
	}

	void FirstApp::run() {
		readCameraThread = std::thread(&FirstApp::readImageStream, this);
		while (!lveWindow.shouldClose()) 
        {
			glfwPollEvents();
			if (auto commandBuffer = lveRenderer.beginFrame())
			{
				int frameIndex = lveRenderer.getFrameIndex();

				//render
				lveRenderer.beginSwapChainRenderPass(commandBuffer);

				//order here matters
				ImGuiNewFrame();
#ifdef IMGUI_HAS_VIEWPORT
                ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->GetWorkPos());
                ImGui::SetNextWindowSize(viewport->GetWorkSize());
                ImGui::SetNextWindowViewport(viewport->ID);
#else 
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif
                ImGui::Begin("Viewer");
                {
                    auto lock = std::lock_guard(_m);
                    ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Error text place");
                }
                ImGui::End();
				ImGuiRender(commandBuffer);

				lveRenderer.endSwapChainRenderPass(commandBuffer);
				lveRenderer.endFrame();
			}
		}

		vkDeviceWaitIdle(lveDevice.device());
	}
}