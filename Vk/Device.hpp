#pragma once

#include <functional>
#include <memory>
#include <vulkan/vulkan.h>
#include "Definition.hpp"

namespace Vk {

class Instance;

class Device : public DeviceAbstract {
	friend Instance;
public:
	enum DeviceFeatures {
		SamplerAnisotropy = 0b00000001,
		SampleRateShading = 0b00000010
	};
	typedef uint32_t DeviceFeaturesFlags;
	
	struct Queue {
		uint32_t index;
		VkQueue queue;
	};
	
public:
	~Device();

	/**
	 * \brief 创建GPU设备
	 * \param alloc Vk::Instance生成的信息
	 * \param config 需要GPU必须支持的特征
	 * \param preferDeviceType 优先选择的设备，如果没有，则会使用CPU
	 * \return
	 */
	static std::unique_ptr<Device> Create(
		const InstanceInformation alloc, 
		DeviceConfig config,
		VkPhysicalDeviceType preferDeviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
	);
	
protected:
	Device() = default;
private:
	std::vector<Queue> _queues;
	//VkPhysicalDevice _physicalDevice = nullptr;
	//VkDevice _logicalDevice = nullptr;
	//using DeviceAbstract::_logicalDevice;
};

}
