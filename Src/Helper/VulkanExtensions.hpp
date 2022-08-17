#pragma once

#include <string>
#include <vulkan/vulkan.h>
#include "..\lve_device.hpp"

namespace VulkanExtensions
{
	struct CommandPoolOwner
	{
	public:
		CommandPoolOwner(lve::LveDevice& lveDevice);
		~CommandPoolOwner();

		const VkCommandPool getPool() { return pool; };
	private:
		VkCommandPool pool;
		lve::LveDevice& device;
	};
}