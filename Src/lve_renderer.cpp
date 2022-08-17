#pragma once
#include "lve_renderer.hpp"

#include <stdexcept>
#include <array>
#include <Helper/VulkanHelpers.hpp>

namespace lve {

	LveRenderer::LveRenderer(LveWindow& window, LveDevice& device) 
		: 
		lveWindow{ window },
		lveDevice{ device },
		currentImageIndex{ 0 },
		currentFrameIndex{ 0 },
		isFrameStarted{false} 
	{
		recreateSwapChain();
		commandPool = lveDevice.createCommandPool();
		createCommandBuffers();
	}

	LveRenderer::~LveRenderer() 
	{
		freeCommandBuffers();
		vkDestroyCommandPool(lveDevice.device(), commandPool, nullptr);
	}

	void LveRenderer::recreateSwapChain() 
	{
		auto extent = lveWindow.getExtend();
		while (extent.width == 0 || extent.height == 0)
		{
			extent = lveWindow.getExtend();
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(lveDevice.device());
		if (lveSwapChain == nullptr)
		{
			lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent);
		}
		else
		{
			std::shared_ptr<LveSwapChain> oldSwapChain = std::move(lveSwapChain);
			lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, oldSwapChain);

			if (!oldSwapChain->compareSwapFormats(*lveSwapChain.get()))
			{
				throw std::runtime_error("Swap chain image(or depth) fromat has changet!");
			}
		}

		//we`ll come back to this in just a moment
	}

	void LveRenderer::createCommandBuffers() 
	{
		commandBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = commandPool;
		allocateInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		auto result = vkAllocateCommandBuffers(lveDevice.device(), &allocateInfo, commandBuffers.data());
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!" + VulkanHelpers::AsString(result));
		}
	};

	void LveRenderer::freeCommandBuffers() 
	{
		vkFreeCommandBuffers(
			lveDevice.device(),
			commandPool,
			static_cast<uint32_t>(commandBuffers.size()),
			commandBuffers.data()
		);
		commandBuffers.clear();
	}

	VkCommandBuffer LveRenderer::beginFrame() 
	{
		globalFrameCounter++;
		assert(!isFrameStarted && "Can`t call beginFrame while already in progress");

		auto result = lveSwapChain->acquireNextImage(&currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return nullptr;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!" + VulkanHelpers::AsString(result));
		}

		isFrameStarted = true;

		auto commandBuffer = getCurrentCommandBuffer();
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin recording command buffer!" + VulkanHelpers::AsString(result));
		}

		return commandBuffer;
	}

	void LveRenderer::endFrame() 
	{
		assert(isFrameStarted && "Can`t call endFrame while frame is not in in progress");
		auto commandBuffer = getCurrentCommandBuffer();

		auto result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record command buffer!" + VulkanHelpers::AsString(result));
		}

		result = lveSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || lveWindow.wasWindowResized())
		{
			lveWindow.resetWindowResizedFlag();
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!" + VulkanHelpers::AsString(result));
		}

		isFrameStarted = false;
		currentFrameIndex = (currentFrameIndex + 1) % LveSwapChain::MAX_FRAMES_IN_FLIGHT;
	}

	void LveRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) 
	{
		assert(isFrameStarted && "Can`t call beginSwapChainRenderPass while frame is not in in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can`t begin render pass on command buffer from a different frame");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = lveSwapChain->getRenderPass();
		renderPassInfo.framebuffer = lveSwapChain->getFrameBuffer(currentImageIndex);

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = lveSwapChain->getSwapChainExtent();

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.01f, 0.01f, 0.01f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(lveSwapChain->getSwapChainExtent().width);
		viewport.height = static_cast<float>(lveSwapChain->getSwapChainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0,0}, lveSwapChain->getSwapChainExtent() };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void LveRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) 
	{
		assert(isFrameStarted && "Can`t call endSwapChainRenderPass while frame is not in in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can`t end render pass on command buffer from a different frame");

		vkCmdEndRenderPass(commandBuffer);
	}
}