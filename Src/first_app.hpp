#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_renderer.hpp"
#include "lve_descriptors.hpp"

#include <memory>
#include <vector>
#include <thread>
#include <mutex>

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
		LveRenderer lveRenderer{ lveWindow, lveDevice };

		// note: order of declarations matters
		std::unique_ptr<LveDescriptorPool> imGuiPool{};
		std::thread readCameraThread;
		char* _image = nullptr;
		int _imageSize = 0;
		int _arrSize;

		volatile bool _stop = false;

		std::mutex _m;
	};
}