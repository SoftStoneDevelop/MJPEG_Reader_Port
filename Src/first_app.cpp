#pragma once

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H = true
#include "first_app.hpp"
#include <iostream>

namespace lve 
{
    FirstApp::FirstApp() 
    {
		imGuiLayer =
			std::make_unique<ImGuiLayer>(
				lveWindow,
				lveDevice,
				lveRenderer->getSwapChainRenderPass(),
				LveSwapChain::MAX_FRAMES_IN_FLIGHT,
				lveTextureStorage,
				lveRenderer->getCommandPool()
				);
	}

	void FirstApp::run() 
    {
		while (!lveWindow.shouldClose()) 
        {
			glfwPollEvents();

			if (auto commandBuffer = lveRenderer->beginFrame())
			{
				int frameIndex = lveRenderer->getFrameIndex();

				//render
				lveRenderer->beginSwapChainRenderPass(commandBuffer);

				//order here matters
                imGuiLayer->Draw(commandBuffer, lveTextureStorage);

                lveRenderer->endSwapChainRenderPass(commandBuffer);
				lveRenderer->endFrame();
			}
		}

		vkDeviceWaitIdle(lveDevice.device());
	}
}