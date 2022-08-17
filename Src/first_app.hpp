#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_renderer.hpp"

#include <memory>
#include "lve_texture_storage.hpp"
#include <ImGuiLayer.hpp>

namespace lve 
{
	class FirstApp 
	{

	public:
		static constexpr int WIDTH = 1920;
		static constexpr int HEIGHT = 1080;

		FirstApp();

		FirstApp(const FirstApp&) = delete;
		void operator=(const FirstApp&) = delete;

		void run();
	private:		
		LveWindow lveWindow{ WIDTH, HEIGHT, "MJPEG Viewer" };
		LveDevice lveDevice{ lveWindow };
		std::shared_ptr<LveRenderer> lveRenderer = std::make_shared<LveRenderer>(lveWindow, lveDevice);
		LveTextureStorage lveTextureStorage{ lveDevice, lveRenderer };
		std::unique_ptr<ImGuiLayer> imGuiLayer;
	};
}