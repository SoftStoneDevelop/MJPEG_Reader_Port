#pragma once
#include "lve_window.hpp"

#include <stdexcept>
#include <Helper/VulkanHelpers.hpp>

namespace lve {

	LveWindow::LveWindow(int width, int height, std::string name) : width{ width }, height{ height }, windowName{name} {
		initWindow();
	}

	LveWindow::~LveWindow() {
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void LveWindow::initWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) 
	{
		auto result = glfwCreateWindowSurface(instance, window, nullptr, surface);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("faile to create window surface!" + VulkanHelpers::AsString(result));
		}
	}

	void LveWindow::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto lveWindow = reinterpret_cast<LveWindow*>(glfwGetWindowUserPointer(window));
		lveWindow->framebufferResized = true;
		lveWindow->width = width;
		lveWindow->height = height;
	}
}