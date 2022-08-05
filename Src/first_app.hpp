#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_renderer.hpp"
#include "lve_descriptors.hpp"

#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include "lve_texture_storage.hpp"

namespace lve {

	class FirstApp {

	public:
		static constexpr int WIDTH = 1920;
		static constexpr int HEIGHT = 1080;

		FirstApp();
		~FirstApp();

		FirstApp(const FirstApp&) = delete;
		void operator=(const FirstApp&) = delete;

		void run();
	private:
		void readImageStream();
		
		LveWindow lveWindow{ WIDTH, HEIGHT, "MJPEG Viewer" };
		LveDevice lveDevice{ lveWindow };
		std::shared_ptr<LveRenderer> lveRenderer = std::make_shared<LveRenderer>(lveWindow, lveDevice);
		LveTextureStorage lveTextureStorage{ lveDevice, lveRenderer };

		// note: order of declarations matters
		std::unique_ptr<LveDescriptorPool> imGuiPool{};
		std::thread readCameraThread;
		std::atomic<int> cameraIndex = 0;
		volatile bool _stop = false;

		std::mutex _m;
	};
}