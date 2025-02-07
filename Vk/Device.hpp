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
	 * \brief ����GPU�豸
	 * \param alloc Vk::Instance���ɵ���Ϣ
	 * \param config ��ҪGPU����֧�ֵ�����
	 * \param preferDeviceType ����ѡ����豸�����û�У����ʹ��CPU
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
