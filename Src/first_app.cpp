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
#include <format>
#include "Helper/VulkanHelpers.hpp"
#include <algorithm>

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

        if (readCameraThread.joinable())
        {
            readCameraThread.join();
        }
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
                    readAsync = client.ReadAsync(readBuffer + (readSize - index), readBufferSize - (readSize - index));
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

                VkSamplerCreateInfo sampler{};
                sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sampler.magFilter = VK_FILTER_LINEAR;
                sampler.minFilter = VK_FILTER_LINEAR;
                sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                sampler.anisotropyEnable = VK_TRUE;
                sampler.maxAnisotropy = lveDevice.properties.limits.maxSamplerAnisotropy;
                sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                sampler.unnormalizedCoordinates = VK_FALSE;
                sampler.compareEnable = VK_FALSE;
                sampler.compareOp = VK_COMPARE_OP_ALWAYS;

                if (!lveTextureStorage.loadTexture(processStart + startData, nextBoundaryIndex, textureCameraName, sampler))
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
        char host[16];
        memset(host, 0, sizeof(host));

        char port[5];
        memset(port, 0, sizeof(port));
        std::vector<int> cameras;

        //TEST
        for (int i = 0; i < 10; i++)
        {
            cameras.push_back(1);
        }

        static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        flags &= ~ImGuiWindowFlags_NoScrollbar;
        auto showWindow = true;
        auto portValide = false;
        auto hostValide = false;

        auto tempValid = false;
		while (!lveWindow.shouldClose()) 
        {
			glfwPollEvents();

			if (auto commandBuffer = lveRenderer->beginFrame())
			{
				int frameIndex = lveRenderer->getFrameIndex();

				//render
				lveRenderer->beginSwapChainRenderPass(commandBuffer);

				//order here matters
				ImGuiNewFrame();

                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->Pos);
                ImGui::SetNextWindowSize(viewport->Size);

                if (ImGui::Begin("Example: Fullscreen window", &showWindow, flags))
                {
                    if (!hostValide)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                    }
                    ImGui::PushItemWidth(100);

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
                    ImGui::SameLine();

                    ImGui::PushItemWidth(50);
                    if (!portValide)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                    }

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
                    ImGui::SameLine();
                    if (ImGui::Button("Add camera"))
                    {
                        //TODO add new thread to vector
                        cameras.push_back(++cameraIndex);
                    }

                    auto frameText = 
                        std::format(
                            "Application average {:.3f}f ms/frame ({:.1f}f FPS)", 
                            1000.0f / ImGui::GetIO().Framerate,
                            ImGui::GetIO().Framerate
                        );
                    auto textSize = ImGui::CalcTextSize(frameText.c_str());;
                    ImGui::BeginListBox("Cameras", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y - textSize.y));
                    ImGui::PushID("##VerticalScrolling");
                    for (int i = 0; i < cameras.size(); i++)
                    {
                        auto textureCameraName = std::format("currentCamera{}Frame", cameras[i]);
                        auto lock = std::lock_guard(_m);
                        if (lveTextureStorage.ContainTexture(textureCameraName))
                        {
                            auto& tData = lveTextureStorage.getTextureData(textureCameraName);
                            auto info = lveTextureStorage.getDescriptorSet(textureCameraName, defaultSamplerName);
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

                        if (ImGui::Button("Close camera"))
                        {
                            //TODO
                        }
                    }
                    ImGui::PopID();
                    ImGui::EndListBox();
                    ImGui::Text(frameText.c_str());

                    ImGui::End();
                }

				ImGuiRender(commandBuffer);

				lveRenderer->endSwapChainRenderPass(commandBuffer);
				lveRenderer->endFrame();
			}
		}

		vkDeviceWaitIdle(lveDevice.device());
	}

    bool FirstApp::validateHost(const char* input)
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

    bool FirstApp::validatePort(const char* input)
    {
        int position = 0;
        char numbers[10] = {'0','1','2','3','4','5','6','7','8','9'};

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