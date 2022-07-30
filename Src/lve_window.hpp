#pragma once
#define GLFW_INCLUDE_VULKAN

#include <glfw/glfw3.h>
#include <vulkan/vulkan.h>

#include <string>

namespace lve {
	class LveWindow
	{
	public:
		LveWindow(int width, int height, std::string name);
		~LveWindow();

		LveWindow(const LveWindow&) = delete;
		void operator=(const LveWindow&) = delete;

		bool shouldClose() { return glfwWindowShouldClose(window); }
		VkExtent2D getExtend() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }; };
		bool wasWindowResized() { return framebufferResized; }
		void resetWindowResizedFlag() { framebufferResized = false; }
		GLFWwindow* getGLFWwindow() const { return window; }

		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
	private:
		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
		void initWindow();

		int width;
		int height;
		bool framebufferResized = false;

		std::string windowName;
		GLFWwindow* window;
	};
}