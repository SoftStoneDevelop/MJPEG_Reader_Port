#pragma once

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H = true
#include "first_app.hpp"
#include "lve_buffer.hpp"

#include "imgui.hpp"
#include "HttpClient.hpp"
#include <ArrayPool.hpp>
#include "Helper/ArrayExt.hpp"
#include <iostream>
#include "Definitions/DefaultSamplersNames.hpp"
#include <fstream>

namespace lve 
{
    FirstApp::FirstApp() 
    {
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

		InitializeImGui(lveWindow, lveDevice, lveRenderer->getSwapChainRenderPass(), imGuiPool->getDescriptorPool(), LveSwapChain::MAX_FRAMES_IN_FLIGHT);
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
        auto pool = ArrayPool::ArrayPool<char>();
        ClientMJPEG::HttpClient client("201.174.12.243", 1024);
        std::string error;
        client.Connect(&error);
        client.SendRequestGetOnStream("/mjpg/video.mjpg", &error);

        std::string boundaryMark = "boundary=";

        auto textureCameraName = std::format("currentCamera{}Frame", ++cameraIndex);

        int readBufferSize = 4000;
        int realSize;
        char* readBuffer = pool.Rent(readBufferSize, realSize);
        readBufferSize = realSize;

        std::future<int> readAsync = client.ReadAsync(readBuffer, readBufferSize);

        int indxBoundary = 0;
        auto foundMark = false;
        while (!_stop)
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
                    if (indxBoundary == boundaryMark.size() - 1)
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
                    readAsync = client.ReadAsync(readBuffer + (readSize - index), readBufferSize - index);
                }
                
                break;
            }

            readAsync = client.ReadAsync(readBuffer, readBufferSize);
        }

        char boundary[70]{};
        int boundarySize = 0;
        int payloadSize = 0;
        while (!_stop)
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
                        readAsync = client.ReadAsync(readBuffer + i, readBufferSize - i);
                        payloadSize = i;
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

        const char* startDataMark = "\r\n\r\n";
        while (!_stop)
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

            {
                auto guard = std::lock_guard(_m);
                lveTextureStorage.unloadTexture(textureCameraName);
                if (!lveTextureStorage.loadTexture(processStart + startData, nextBoundaryIndex, textureCameraName))
                {
                    std::cout << "Image fail with size:" << nextBoundaryIndex << std::endl;
                }
            }

            payloadSize -= startData + nextBoundaryIndex;
            payloadOffset += startData + nextBoundaryIndex;

            std::cout << "Image with size:" << nextBoundaryIndex << std::endl;
            std::cout << std::endl;
        }

        pool.Return(imageBuffer);
        pool.Return(readBuffer);
        pool.Return(boundary);
	}

	void FirstApp::run() 
    {
		readCameraThread = std::thread(&FirstApp::readImageStream, this);

        auto textureCameraName = std::format("currentCamera{}Frame", 1);

        auto currentTime = std::chrono::high_resolution_clock::now();
		while (!lveWindow.shouldClose()) 
        {
			glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            long frameTime = std::chrono::duration<float, std::chrono::milliseconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            if (frameTime < 33)
            {
                //std::cout << "Frame time:{} milleseconds" << frameTime << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(33 - frameTime));
            }

			if (auto commandBuffer = lveRenderer->beginFrame())
			{
				int frameIndex = lveRenderer->getFrameIndex();

				//render
				lveRenderer->beginSwapChainRenderPass(commandBuffer);

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
                    if (lveTextureStorage.ContainTexture(textureCameraName))
                    {
                        auto& tData = lveTextureStorage.getTextureData(textureCameraName);
                        auto info = lveTextureStorage.getDescriptorSet(textureCameraName, defaultSamplerName);
                        ImGui::Image(info, { (float)tData.texWidth, (float)tData.texHeight });
                    }

                    ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Error text place");
                }
                ImGui::End();
				ImGuiRender(commandBuffer);

				lveRenderer->endSwapChainRenderPass(commandBuffer);
				lveRenderer->endFrame();
			}
		}

		vkDeviceWaitIdle(lveDevice.device());
	}
}