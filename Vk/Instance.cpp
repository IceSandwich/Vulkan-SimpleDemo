#include "Instance.hpp"

#include <algorithm>
#include <iostream>
#include <glog/logging.h>
#include "Definition.hpp"

using namespace Vk;

std::unique_ptr<Instance> Instance::Create(const VkApplicationInfo applicationInfo, std::vector<const char*> enabledExtensionNames, const PFN_vkDebugUtilsMessengerCallbackEXT debugUtilsMessengerCallback) {
	std::unique_ptr<Instance> ret = std::unique_ptr<Instance>(new Instance());

	LOG_IF(INFO, debugUtilsMessengerCallback == nullptr) << "Run with validation layers disabled.";
	const bool enableValidationLayers = debugUtilsMessengerCallback && checkValidationLayerSupport();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, nullptr };
	debugCreateInfo.flags = 0;
	debugCreateInfo.messageSeverity = VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugUtilsMessengerCallback;
	debugCreateInfo.pUserData = nullptr;

	VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr };
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
		createInfo.pNext = &debugCreateInfo;
		if (std::find(enabledExtensionNames.begin(), enabledExtensionNames.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == enabledExtensionNames.end()) {
			enabledExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}
	createInfo.enabledExtensionCount = enabledExtensionNames.size();
	createInfo.ppEnabledExtensionNames = enabledExtensionNames.data();
	createInfo.pApplicationInfo = &applicationInfo;

	VkResult status = vkCreateInstance(&createInfo, nullptr, &ret->_instance);
	CHECK(status == VK_SUCCESS) << "Failed to create instance! Error code: " << status;

#pragma region Print Available Extensions
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> vkExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, vkExtensions.data());
		
		std::stringstream ss;
		ss << "Available extensions: " << std::endl;
		for (const auto &vkExtension : vkExtensions) {
			ss << '\t' << vkExtension.extensionName << std::endl;
		}
		LOG(INFO) << std::endl << ss.str();
	}
#pragma endregion
	
#pragma region Setup Debug Messenger
	if (enableValidationLayers) {
		PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(ret->_instance, "vkCreateDebugUtilsMessengerEXT"));
		CHECK(func != nullptr) << "Can't find vkCreateDebugUtilsMessengerEXT function!";

		VkResult status = func(ret->_instance, &debugCreateInfo, nullptr, &ret->_debugMessenger);
		CHECK(status == VK_SUCCESS) << "Failed to setup debug messenger! Error code: " << status;
	}
#pragma endregion
	
	return ret;
}

InstanceInformation Instance::GenerateInstanceInformation() {
	return InstanceInformation{
		_instance,
		_debugMessenger != nullptr,
	};
}

VkSurfaceKHR Instance::CreateWindowSurface(GLFWwindow* window) {
	VkSurfaceKHR surface;
	VkResult status = glfwCreateWindowSurface(_instance, window, nullptr, &surface);
	CHECK(status == VK_SUCCESS) << "Failed to create window surface!";
	return surface;

}

Instance::~Instance() {
	if (_debugMessenger != nullptr) {
		PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (func != nullptr) {
			func(_instance, _debugMessenger, nullptr);
		} else {
			LOG(ERROR) << "Can't find vkDestroyDebugUtilsMessengerEXT function!";
		}
	}
	
	if (_instance != nullptr) {
		vkDestroyInstance(_instance, nullptr);
		_instance = nullptr;
	}
}

bool Instance::checkValidationLayerSupport() {
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// check if validationLayers is a subset of availableLayers
	const bool ret = std::all_of(validationLayers.begin(), validationLayers.end(), [&availableLayers](const char *layerName) -> bool {
		return std::any_of(availableLayers.begin(), availableLayers.end(), [&layerName](VkLayerProperties &layerProperty) -> bool {
			return strcmp(layerName, layerProperty.layerName) == 0;
		});
	});

#pragma region Print Available Layers
	{
		std::stringstream ss;
		ss << "Available layers: " << std::endl;
		for (const auto &layer : availableLayers) {
			ss << '\t' << layer.layerName << std::endl;
		}
		LOG(INFO) << std::endl << ss.str();
	}
#pragma endregion 

	if (!ret) {
		LOG(WARNING) << "Validation layer is unavailable in this machine. Disable it.";
	}

	return ret;

}
