/**
 * @author gh Corgice @IceSandwich
 * @date August 2023
 * @license MIT
 */

#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <set>
#include <GLFW/glfw3native.h>

#include <glog/logging.h>

namespace vk::toolkit {

static vk::ApplicationInfo DefaultApplicationInfo{"Vulkan Application", VK_MAKE_VERSION(1, 0, 0), "VkToolkit", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0};

struct Instance {
	vk::raii::Instance instance;
	vk::raii::DebugUtilsMessengerEXT debugMessenger;
	Instance(const vk::raii::Context &context, std::vector<const char *> enabledExtensionNames = {}, const PFN_vkDebugUtilsMessengerCallbackEXT debugUtilsMessengerCallback = nullptr, const vk::ApplicationInfo applicationInfo = DefaultApplicationInfo) : instance(nullptr), debugMessenger(nullptr) {
		LOG_IF(INFO, debugUtilsMessengerCallback == nullptr) << "Run with validation layers disabled.";

		std::vector<vk::LayerProperties> availableLayers = context.enumerateInstanceLayerProperties();
		if (std::ostringstream ss; true) {
			ss << "Available layers: " << std::endl;
			for (const vk::LayerProperties &layer : availableLayers) ss << std::setw(10) << ' ' << layer.layerName << std::endl;
			LOG(INFO) << std::endl << ss.str();
		}

		std::vector<vk::ExtensionProperties> availableExtensions = context.enumerateInstanceExtensionProperties();
		if (std::ostringstream ss; true) {
			ss << "Available extensions: " << std::endl;
			for (const vk::ExtensionProperties &layer : availableExtensions) ss << std::setw(10) << ' ' << layer.extensionName << std::endl;
			LOG(INFO) << std::endl << ss.str();
		}

		bool enableValidationLayers = debugUtilsMessengerCallback && std::find_if(availableLayers.begin(), availableLayers.end(), [](const vk::LayerProperties &prop) -> bool {
			return (strcmp(prop.layerName, "VK_LAYER_KHRONOS_validation") == 0);
			}) != availableLayers.end() && std::find_if(availableExtensions.begin(), availableExtensions.end(), [](const vk::ExtensionProperties &prop) -> bool {
				return (strcmp(prop.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0);
				}) != availableExtensions.end();
				LOG(INFO) << "Use validation layers: " << std::boolalpha << enableValidationLayers << std::noboolalpha << std::endl;

				constexpr std::array<const char *, 1> layerNames = {"VK_LAYER_KHRONOS_validation"};
				if (enableValidationLayers && std::find(enabledExtensionNames.begin(), enabledExtensionNames.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == enabledExtensionNames.end()) {
					enabledExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				}
				if (std::ostringstream ss; true) {
					ss << "Require extensions: " << std::endl;
					for (const char *extensionName : enabledExtensionNames) ss << std::setw(10) << ' ' << extensionName << std::endl;
					LOG(INFO) << std::endl << ss.str();
				}
				const vk::InstanceCreateInfo instanceCreateInfo{{}, &applicationInfo, enableValidationLayers ? layerNames.size() : 0, layerNames.data(), enabledExtensionNames.size(), enabledExtensionNames.data()};

				instance = std::move(vk::raii::Instance{context, instanceCreateInfo});

				if (enableValidationLayers) {
					const vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
					const vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
					const vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, debugUtilsMessengerCallback);

					debugMessenger = std::move(vk::raii::DebugUtilsMessengerEXT{instance, debugUtilsMessengerCreateInfoEXT});
				}
	}
};

struct QueueFlag {
	enum Bits : unsigned {
		Graphics = VK_QUEUE_GRAPHICS_BIT,
		Compute = VK_QUEUE_COMPUTE_BIT,
		Transfer = VK_QUEUE_TRANSFER_BIT,
		SparseBinding = VK_QUEUE_SPARSE_BINDING_BIT,
		Protected = VK_QUEUE_PROTECTED_BIT,
		VideoDecodeKHR = VK_QUEUE_VIDEO_DECODE_BIT_KHR,
		OpticalFlowNV = VK_QUEUE_OPTICAL_FLOW_BIT_NV,
		PresentKHR = static_cast<unsigned>(OpticalFlowNV) << 2 //for safe?
	};
	static vk::QueueFlags toVkQueueFlags(Bits flags) {
		return vk::QueueFlags{static_cast<vk::QueueFlagBits>(flags & ~Bits::PresentKHR)};
	}
	static vk::QueueFlagBits toVkQueueFlagBits(Bits flags) {
		return static_cast<vk::QueueFlagBits>(flags & ~Bits::PresentKHR);
	}
	static std::string toString(Bits flags) {
		if (flags == Bits::PresentKHR) {
			return std::string{"{ Present KHR }"};
		}
		if (flags & Bits::PresentKHR) {
			return std::string{"{ Present KHR |"} + vk::to_string(toVkQueueFlags(flags)).substr(1);
		}
		return vk::to_string(toVkQueueFlags(flags));
	}
};
constexpr QueueFlag::Bits operator|(const QueueFlag::Bits left, const QueueFlag::Bits right) {
	return static_cast<QueueFlag::Bits>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

template <QueueFlag::Bits ...Flags>
struct QueueFamily {
	constexpr static size_t NumOfFlags = sizeof...(Flags);
	std::array<uint32_t, NumOfFlags> indices;

private:
	struct queuePropFinder {
		int i;
		QueueFlag::Bits target;
		vk::PhysicalDevice device;
		vk::SurfaceKHR surface;
		queuePropFinder(QueueFlag::Bits target, vk::PhysicalDevice device, vk::SurfaceKHR surface = nullptr): i(-1), target(target), device(device), surface(surface) {
			if (target & QueueFlag::PresentKHR) {
				CHECK(surface) << "Query queue " << QueueFlag::toString(target) << " need present KHR check, but surface passed to toolkit::QueueFamily is nullptr.";
			}
		}
		bool operator()(const vk::QueueFamilyProperties &prop) {
			++i;
			
			if (target & QueueFlag::PresentKHR) {
				if (!device.getSurfaceSupportKHR(i, surface)) return false;

				if (target == QueueFlag::PresentKHR) return true;
			}
			return static_cast<uint32_t>(prop.queueFlags & QueueFlag::toVkQueueFlagBits(target)) > 0;

		}
	};
public:

	static bool Check(const vk::raii::PhysicalDevice &physicalDevice, vk::SurfaceKHR surface = nullptr) {
		std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();

		auto check = [&queueProps, &physicalDevice, &surface](QueueFlag::Bits flag) {
			auto queuePropIter = std::find_if(queueProps.begin(), queueProps.end(), queuePropFinder{flag, *physicalDevice, surface});
			return queuePropIter != queueProps.end();
		};

		return (... & check(Flags));
	}
	
	QueueFamily(const vk::raii::PhysicalDevice &physicalDevice, vk::SurfaceKHR surface = nullptr) {
		std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();

		auto queryQueueFamilyIndex = [&queueProps, &surface, &physicalDevice](QueueFlag::Bits flag) -> uint32_t {
			auto queuePropIter = std::find_if(queueProps.begin(), queueProps.end(), queuePropFinder{flag, *physicalDevice, surface});
			CHECK(queuePropIter != queueProps.end()) << "Can't find query queue: " << QueueFlag::toString(flag);
			const uint32_t index = std::distance(queueProps.begin(), queuePropIter);
			LOG(INFO) << "Query queue " << QueueFlag::toString(flag) << " at index " << index;
			return index;
		};

		indices = {queryQueueFamilyIndex(Flags)...};
	}
	
	std::vector<vk::DeviceQueueCreateInfo> GenerateDefaultQueueCreateInfo() {
		static float queuePriority = 0;

		const std::set<uint32_t> uniqueQueueFamilies = {indices.begin(), indices.end()};
		
		std::vector<vk::DeviceQueueCreateInfo> ret;
		std::transform(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end(), std::back_inserter(ret), [](const uint32_t index) {
			return vk::DeviceQueueCreateInfo{{}, index, 1, &queuePriority};
		});
		return ret;
	}

	std::array<vk::raii::Queue, NumOfFlags> FetchQueues(const vk::raii::Device &device) {
		return { [i = -1, &device, this](decltype(Flags)) mutable {
			return vk::raii::Queue{device, indices[++i], 0};
		}(Flags)... };
	}
};

struct SwapChain {
	static bool Check(const vk::raii::PhysicalDevice device) {
		std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();
		return std::find(availableExtensions.begin(), availableExtensions.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME) != availableExtensions.end();
	}
};

struct WindowData {
	std::string name;
	vk::Extent2D extent;
	WindowData(const std::string name, const vk::Extent2D extent) : name(name), extent(extent) {

	}
	virtual ~WindowData() = default;
	virtual VkSurfaceKHR CreateSurface(const VkInstance instance) = 0;
	virtual std::vector<const char *> GetRequiredInstanceExtensions() = 0;
};
struct GLFWWindow : WindowData {
	GLFWwindow *handle;
	GLFWWindow(const std::string name, vk::Extent2D extent) : WindowData{name, extent}, handle(nullptr) {
		static struct glfwContext {
			glfwContext() {
				glfwInit();
				glfwSetErrorCallback([](int error, const char *msg) {
					LOG(ERROR) << "GLFW error: " << error << ", msg: " << msg << std::endl;
					});
			}
			~glfwContext() {
				glfwTerminate();
			}
		} glfwCtx{};
		(void)glfwCtx;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		handle = glfwCreateWindow(extent.width, extent.height, name.c_str(), nullptr, nullptr);
		CHECK(handle != nullptr) << "Can't create glfw window";
	}
	~GLFWWindow() override {
		if (handle) {
			glfwDestroyWindow(handle);
		}
	}
	VkSurfaceKHR CreateSurface(const VkInstance instance) override {
		VkSurfaceKHR _surface;
		CHECK(glfwCreateWindowSurface(instance, handle, nullptr, &_surface) == VK_SUCCESS) << "Failed to create window surface!";
		return _surface;
	}
	std::vector<const char *> GetRequiredInstanceExtensions() override {
		uint32_t glfwExtensionCount = 0;
		const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		return std::vector<const char *>(glfwExtensions, glfwExtensions + glfwExtensionCount);
	}
};

struct SurfaceData {
	SurfaceData(const vk::raii::Instance &instance, WindowData *data)
		: extent(data->extent), window(data), surface{instance, data->CreateSurface(static_cast<vk::Instance>(*instance))} {

	}

	vk::Extent2D         extent;
	WindowData *window;
	vk::raii::SurfaceKHR surface = nullptr;
};
vk::raii::PhysicalDevice PickPhysicalDevice(const vk::raii::Instance &instance, std::function<bool(const vk::raii::PhysicalDevice &device, const vk::PhysicalDeviceProperties &, const std::vector<vk::QueueFamilyProperties> &queueFamilyPropertieses)> func) {
	vk::raii::PhysicalDevices devices = vk::raii::PhysicalDevices{instance};

	int preferDevice = -1;

	std::ostringstream ss;
	ss << "Found devices:" << std::endl;
	for (size_t i = 0; i < devices.size(); ++i) {
		const vk::raii::PhysicalDevice &device = devices[i];
		vk::PhysicalDeviceProperties prop = device.getProperties();
		std::vector<vk::QueueFamilyProperties> queueFamilyPropertieses = device.getQueueFamilyProperties();

		if (preferDevice == -1 && func(device, prop, queueFamilyPropertieses)) preferDevice = i;

		constexpr static size_t indent = 4 + 20;
		ss << "[" << (preferDevice == i ? 'X' : ' ') << "] " << prop.deviceName << std::endl;
		ss << std::setw(indent) << "Api version: " << ((prop.apiVersion >> 22) & 0xfff) << '.' << ((prop.apiVersion >> 12) & 0x3ff) << '.' << (prop.apiVersion & 0xfff) << std::endl;
		ss << std::setw(indent) << "Driver version : " << prop.driverVersion << std::endl;

		ss << std::showbase << std::internal;
		ss << std::setw(indent) << "Device id : ";
		ss << std::setfill('0') << std::hex << std::setw(6) << prop.deviceID << std::setfill(' ') << std::endl;
		ss << std::setw(indent) << "Vendor id : ";
		ss << std::setfill('0') << std::hex << std::setw(6) << prop.vendorID << std::setfill(' ') << std::endl;
		ss << std::noshowbase << std::right << std::dec;

		ss << std::setw(indent) << "Device type : " << vk::to_string(prop.deviceType) << std::endl;

		ss << std::setw(indent) << "Queue family : ";
		for (const vk::QueueFamilyProperties &queueProp : device.getQueueFamilyProperties()) {
			ss << vk::to_string(queueProp.queueFlags) << ", ";
		}
		ss << std::endl;
	}

	LOG(INFO) << ss.str();

	CHECK(preferDevice != -1) << "Can't find suitable device!";

	return devices[preferDevice];
}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		// Message is important enough to show
		std::cout << "-- validation layer: " << pCallbackData->pMessage << std::endl;
	}
	return VK_FALSE; // return true to abort
}
