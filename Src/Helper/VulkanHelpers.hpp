#pragma once

#include <string>
#include <vulkan/vulkan.h>

namespace lve
{
	namespace VulkanHelpers
	{
		std::string AsString(VkResult& result);
	}
}