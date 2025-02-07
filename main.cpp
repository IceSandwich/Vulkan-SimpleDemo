/**
 * @author gh Corgice @IceSandwich
 * @date August 2023
 * @license MIT
 */

#include <iomanip>

//#include "Vk/Instance.hpp"
#include <fstream>

#include "../TemplateTest/defer.hpp"

#include <iostream>
#include <glog/logging.h>

#include "vktoolkit.hpp"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

void run2() {
	//auto instance = Vk::Instance::Create(Vk::Generators::GenerateApplicationInfo(), {}, debugCallback);
	//CHECK(instance) << "Can't create instance!";

	//auto device = Vk::Device::Create(instance->GenerateInstanceInformation(), Vk::Generators::GenerateComputeDeviceConfig());

}

void run() {
	vk::toolkit::GLFWWindow window{"Hello glfw", vk::Extent2D{800, 600}};
	
#pragma region Create device
	const vk::raii::Context context;
	vk::toolkit::Instance instance{context, window.GetRequiredInstanceExtensions(), debugCallback};

	vk::toolkit::SurfaceData surface{instance.instance, &window};

	using QueueFamily = vk::toolkit::QueueFamily<vk::toolkit::QueueFlag::Graphics | vk::toolkit::QueueFlag::Compute, vk::toolkit::QueueFlag::PresentKHR>;
	
	vk::raii::PhysicalDevice physicalDevice = vk::toolkit::PickPhysicalDevice(instance.instance, [&surface](const vk::raii::PhysicalDevice &device, const vk::PhysicalDeviceProperties &prop, const std::vector<vk::QueueFamilyProperties> queueFamily) -> bool {
		return QueueFamily::Check(device, *surface.surface);
	});
	
	QueueFamily queueFamily{physicalDevice, *surface.surface};
	auto queueCreateInfo = queueFamily.GenerateDefaultQueueCreateInfo();
	vk::raii::Device device{physicalDevice, vk::DeviceCreateInfo{{}, queueCreateInfo.size(), queueCreateInfo.data()}};
	
	auto queues = queueFamily.FetchQueues(device);
	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
#pragma endregion 
	
	vk::raii::CommandPool commandPool{device, vk::CommandPoolCreateInfo{ {}, queueFamily.indices[0] } };


	
	//ImGui::CreateContext();
	//ImGui_ImplGlfw_InitForVulkan(window.handle, true);

	//ImGui_ImplVulkan_InitInfo initINfo{
	//	*instance.instance,
	//	*physicalDevice,
	//	*device,
	//	queueFamilyIndices[0],
	//	*queues[0],
	//	nullptr,
	//	
	//}

	while (!glfwWindowShouldClose(window.handle)) {
		glfwPollEvents();

	}

}

struct GLog {
	GLog(const int &argc, char ** const &argv) {
		google::SetLogDestination(google::GLOG_INFO, "Logs");
		google::SetStderrLogging(google::GLOG_INFO);
		google::SetLogFilenameExtension(".log");
		google::InitGoogleLogging(argv[0]);
		google::InstallFailureWriter(&CoreDumpCatcher);
	}
	~GLog() {
		google::ShutdownGoogleLogging();
	}
private:
	static void CoreDumpCatcher(const char *data, size_t size) {
		std::string msg{data, size};
		
		std::ofstream s{"FATAL.log"};
		s << "Fatal information: " << msg;
	}
};

int main(int argc, char* argv[]) {
	GLog logging{argc, argv};

	run();
	system("pause");
	
	return 0;
}
