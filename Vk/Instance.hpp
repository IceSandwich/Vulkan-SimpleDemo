#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>



#include "Device.hpp"

namespace Vk {

	class Instance {
	public: /* static */
		static std::unique_ptr<Instance> Create(const VkApplicationInfo applicationInfo, const std::vector<const char*> enabledExtensionNames, const PFN_vkDebugUtilsMessengerCallbackEXT debugUtilsMessengerCallback);
		InstanceInformation GenerateInstanceInformation();
		VkSurfaceKHR CreateWindowSurface(GLFWwindow *window);
	public: /* class */		
		~Instance();
	protected:
		Instance() = default;
		
		static bool checkValidationLayerSupport();
	private:
		VkInstance _instance = nullptr;
		[[maybe_unused]] VkDebugUtilsMessengerEXT _debugMessenger = nullptr;
	};
}
