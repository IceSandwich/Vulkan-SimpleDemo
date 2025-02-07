#pragma once
#include <glog/logging.h>
#include <vulkan/vulkan.h>
#include "../Definition.hpp"

namespace Vk {
class StagingBuffer {
public:
	StagingBuffer(DeviceAbstract device, void *data, VkDeviceSize size);
	
protected:
	void createBuffer(VkDeviceSize size) {
		VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0};
		bufferInfo.size = size;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkResult status = vkCreateBuffer(_device, &bufferInfo, nullptr, &_buffer);
		CHECK(status == VK_SUCCESS) << "Failed to create buffer!";
		
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(_device, _buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr};
		allocInfo.allocationSize = memRequirements.size;
		//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		status = vkAllocateMemory(_device, &allocInfo, nullptr, &_memory);
		CHECK(status == VK_SUCCESS) << "Failed to allocate vertex buffer memory!";

		vkBindBufferMemory(device, buffer, bufferMemory, 0);
	}

private:
	VkDevice _device;
	VkBuffer _buffer;
	VkDeviceMemory _memory;
};
}