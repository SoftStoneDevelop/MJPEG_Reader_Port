#pragma once

#include "VulkanExtensions.hpp"

namespace VulkanExtensions
{
	CommandPoolOwner::CommandPoolOwner(lve::LveDevice& lveDevice) : device{ lveDevice }
	{
		pool = device.createCommandPool();
	}

	CommandPoolOwner::~CommandPoolOwner()
	{
		vkDestroyCommandPool(device.device(), pool, nullptr);
	}
}