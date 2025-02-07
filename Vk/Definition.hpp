#pragma once
#include <functional>
#include <vulkan/vulkan.h>
#include <memory.h>

namespace Vk {

struct InstanceInformation {	
	VkInstance instance;
	bool enableValidationLayers;
};


constexpr static std::array<const char *, 1> validationLayers{
	"VK_LAYER_KHRONOS_validation"
};

struct DeviceConfig {
	std::vector<const char *> instanceExtensions;
	std::vector<VkQueueFlags> requiredQueueFamily;
	VkPhysicalDeviceFeatures requiredFeatures;
	std::vector<const char *> deviceExtensions;

	DeviceConfig();
};

struct Generators {
	static VkApplicationInfo GenerateApplicationInfo(const char* appName = "");
	static DeviceConfig GenerateGraphicDeviceConfig(bool withGLFW = true);
	static DeviceConfig GenerateComputeDeviceConfig();
};

class DeviceAbstract {
public:
	VkPhysicalDevice _physicalDevice = nullptr;
	VkDevice _logicalDevice = nullptr;
};


}
