#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_renderer.hpp"
#include "lve_descriptors.hpp"

#include <memory>
#include <vector>

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
		
		LveWindow lveWindow{ WIDTH, HEIGHT, "MJPEG Viewer" };
		LveDevice lveDevice{ lveWindow };
		LveRenderer lveRenderer{ lveWindow, lveDevice };

		// note: order of declarations matters
		std::unique_ptr<LveDescriptorPool> imGuiPool{};
	};
}