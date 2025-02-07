#include "Definition.hpp"

#include <GLFW/glfw3.h>

namespace Vk {

DeviceConfig::DeviceConfig() {
	memset(&requiredFeatures, 0, sizeof(requiredFeatures));
}

VkApplicationInfo Generators::GenerateApplicationInfo(const char* appName) {
	VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr};
	appInfo.apiVersion = VK_API_VERSION_1_0;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pApplicationName = appName;
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No engine";
	return appInfo;
}

DeviceConfig Generators::GenerateGraphicDeviceConfig(bool withGLFW) {
	DeviceConfig config;
	if (withGLFW) {
		uint32_t glfwExtensionCount = 0;
		const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		config.instanceExtensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
	}
	config.requiredQueueFamily.emplace_back(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
	config.requiredFeatures.samplerAnisotropy = VK_TRUE;
	config.requiredFeatures.sampleRateShading = VK_TRUE;
	config.deviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	config.deviceExtensions.emplace_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
	return config;
}

DeviceConfig Generators::GenerateComputeDeviceConfig() {
	DeviceConfig config;
	config.requiredQueueFamily.emplace_back(VK_QUEUE_COMPUTE_BIT);
	config.deviceExtensions.emplace_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
	return config;
}

}
