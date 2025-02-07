#include "Device.hpp"
#include "Device.hpp"

#include <glog/logging.h>
#include <algorithm>
#include <numeric>
#include <set>

#include <unordered_set>

using namespace Vk;

std::unique_ptr<Device> Device::Create(const InstanceInformation alloc, DeviceConfig config, VkPhysicalDeviceType preferDeviceType) {
	std::unique_ptr<Device> ret = std::unique_ptr<Device>(new Device);

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(alloc.instance, &deviceCount, nullptr);
	CHECK(deviceCount != 0) << "Failed to find GPUs with Vulkan support!";

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(alloc.instance, &deviceCount, devices.data());

#pragma region Pick Physical Device
	int preferDevice = -1;
	std::vector<std::string> deviceNames;
	std::stringstream ss;
	ss << "Found devices:" << std::endl;

	for (size_t i = 0; i < devices.size(); ++i) {
		const VkPhysicalDevice &device = devices[i];

#pragma region Fetch Device Information
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(device, &prop);

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
#pragma endregion

#pragma region Prepare Log
		deviceNames.emplace_back(prop.deviceName);
		ss << "\t[" << i << "] " << prop.deviceName << ":" << std::endl;
		ss << "\t    Device id: " << prop.deviceID << std::endl;
		ss << "\t    Device type: ";
		switch (prop.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: ss << "Other"; break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: ss << "Integrated GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: ss << "Discrete GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: ss << "Virtual GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: ss << "CPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM: ss << "?"; break;
		default: ss << "Unknown"; break;
		}
		ss << std::endl;
		ss << "\t    Queue family: ";
		for (const VkQueueFamilyProperties& queueProp : queueFamilies) {
			if (queueProp.queueFlags & VK_QUEUE_GRAPHICS_BIT) ss << "Graphics + ";
			if (queueProp.queueFlags & VK_QUEUE_COMPUTE_BIT) ss << "Compute + ";
			if (queueProp.queueFlags & VK_QUEUE_TRANSFER_BIT) ss << "Transfer + ";
			if (queueProp.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ss << "Sparse binding + ";
			if (queueProp.queueFlags & VK_QUEUE_PROTECTED_BIT) ss << "Protected + ";
			if (queueProp.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) ss << "Video decode + ";
			if (queueProp.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV) ss << "Optical flow + ";
			ss << "(" << queueProp.queueCount << "), ";
		}
		ss << std::endl;

		//TODO: presentation queue
#pragma endregion 

#pragma region Filter Devices
		if ((prop.deviceType & preferDeviceType) == 0) continue;

		if (std::all_of(config.requiredQueueFamily.begin(), config.requiredQueueFamily.end(), [&queueFamilies](const VkQueueFlags requiredFlags) -> bool{
			return std::any_of(queueFamilies.begin(), queueFamilies.end(), [requiredFlags](const VkQueueFamilyProperties prop) -> bool {
				return prop.queueFlags & requiredFlags;
				// TODO: vkGetPhysicalDeviceSurfaceSupportKHR
			});
		}) == false) continue;

		if (preferDevice == -1) {
			preferDevice = i;
		}
#pragma endregion 
	}
	
	LOG(INFO) << "Select device: #" << preferDevice << std::endl << ss.str();
	CHECK(preferDevice != -1) << "Failed to find a suitable GPU!";
	
	ret->_physicalDevice = devices[preferDevice];
#pragma endregion

#pragma region Create Logical Device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(ret->_physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(ret->_physicalDevice, &queueFamilyCount, queueFamilies.data());

	std::transform(config.requiredQueueFamily.begin(), config.requiredQueueFamily.end(), std::back_inserter(ret->_queues), [&queueFamilies](const VkQueueFlags flags) -> Queue {
		for (size_t i = 0; i < queueFamilies.size(); ++i) {
			if (flags & queueFamilies[i].queueFlags) return Queue{static_cast<uint32_t>(i), nullptr};
		}

		LOG(FATAL) << "Shouldn't get here!";
		return Queue{};
	});

	std::set<int32_t> uniqueQueueIndices;
	for (const Queue& q : ret->_queues) {
		uniqueQueueIndices.insert(q.index);
	}

	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	for (int32_t queueIndex : uniqueQueueIndices) {
		VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0};
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.queueFamilyIndex = queueIndex;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0};
	createInfo.enabledExtensionCount = static_cast<uint32_t>(config.deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = config.deviceExtensions.data();
	if (alloc.enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}
	createInfo.pEnabledFeatures = &config.requiredFeatures;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	int status = vkCreateDevice(ret->_physicalDevice, &createInfo, nullptr, &ret->_logicalDevice);
	CHECK(status == VK_SUCCESS) << "Failed to create logical device!";

	for (Queue &q : ret->_queues) {
		vkGetDeviceQueue(ret->_logicalDevice, q.index, 0, &q.queue);
	}
#pragma endregion 

	return ret;
}

Device::~Device() {
	if (_logicalDevice != nullptr) {
		vkDestroyDevice(_logicalDevice, nullptr);
		_logicalDevice = nullptr;
	}
}
