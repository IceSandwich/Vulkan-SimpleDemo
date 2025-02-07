/**
 * @author gh Corgice @IceSandwich
 * @date Oct 2023
 * @license MIT
 */

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //Vulkan的深度表示是0~1，OpenGL的深度表示是-1~1
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>
#include <fstream>
#include <numeric>
#include <bitset>
#include <array>
#include <iostream>
#include <functional>
#include <opencv2/opencv.hpp>
#include <algorithm>

#include "../TemplateTest/pythonic.hpp"
#include "../TemplateTest/defer.hpp"
#include "../TemplateTest/sequence.hpp"

/* Vulkan中同步概念
Barrier 同一个Queue的Commands之间的依赖关系
Semaphore 一个Submit和另一个Submit之间的依赖关系（可以是同一个queue也可以不同queue）
Fence device的状态

比如：
CommandBuffer中有：
1. vkCmdDispatch(...);
2. vkCmdDispatch(...);
其中，1、2按顺序启动，但是可能1、2是同时运行的，也可能是2先执行再执行1，如果2依赖1的输出结果，那么这样会出错
因此需要加上barrier，限制两个Commands之间的依赖关系
1. vkCmdDispatch(...);
2. vkCmdPipelineBarrier(...);
3. vkCmdDispatch(...);
确保跑3前，1已经跑完

Barrier有三个功能：
1. 名如其实，具有控制依赖关系的功能
2. 转换图像布局
3. 转移queue family所属权（使用VK_QUEUE_FAMILY_IGNORED禁用）
*/

constexpr uint64_t FenceTimeout = 100000000;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		// Message is important enough to show
	}

	LOG(ERROR) << pCallbackData->pMessage;

	return VK_FALSE; // return true to abort
}

constexpr static std::array<const char *, 1> validationLayers{
	"VK_LAYER_KHRONOS_validation"
};
std::vector<const char *> extensionLayers = {
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};

static std::vector<char> readBinary(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		char tmp[250];
		getcwd(tmp, 250);
		LOG(INFO) << "Current work directory: " << tmp << std::endl;

		LOG(FATAL) << "Failed to open file!" << filename;
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	LOG(INFO) << "Read " << filename << " " << fileSize << " Bytes.";

	file.close();
	return buffer;
}


uint32_t findMemoryType(const vk::PhysicalDeviceMemoryProperties &memoryProperties, uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask) {
	LOG(INFO) << "debug memory type for typeBits=" << std::bitset<sizeof(typeBits)>{typeBits} << " and requirementsMask=" << vk::to_string(requirementsMask);
	uint32_t typeIndex = std::numeric_limits<uint32_t>::max();
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++, typeBits >>= 1) {
		if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)) {
			return i;
		}
	}
	LOG(FATAL) << "Can't find suitable memory type for typeBits=" << std::bitset<sizeof(typeBits)>{typeBits} << " and requirementsMask=" << vk::to_string(requirementsMask);
}

struct Buffer {
	vk::raii::Buffer buffer;
	vk::raii::DeviceMemory deviceMemory;
	vk::DeviceSize deviceSize;
	Buffer(const vk::raii::PhysicalDevice &physicalDevice, const vk::raii::Device &device, const vk::DeviceSize deviceSize, const vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags properties) : buffer{nullptr}, deviceMemory{nullptr}, deviceSize(deviceSize) {
		vk::BufferCreateInfo bufferCreateInfo{ {}, deviceSize, usage };
		buffer = vk::raii::Buffer{ device,bufferCreateInfo };
		vk::MemoryRequirements memRequirement = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{ memRequirement.size, findMemoryType(physicalDevice.getMemoryProperties(), memRequirement.memoryTypeBits, properties) };
		deviceMemory = vk::raii::DeviceMemory{ device, allocInfo };

		buffer.bindMemory(*deviceMemory, 0);
	}

	Buffer &UploadData(const void *data) {
		void *mapped = deviceMemory.mapMemory(0, deviceSize);
		memcpy(mapped, data, deviceSize);
		deviceMemory.unmapMemory();
		
		return *this;
	}

	template <typename T>
	std::unique_ptr<T[], std::function<void(T*)> > MapFromDevice() {
		void *mapped = deviceMemory.mapMemory(0, deviceSize);
		return std::unique_ptr<T[], std::function<void(T*)> >(
			(T*)mapped, 
			[mem = *deviceMemory, device = buffer.getDevice()](T*) {
				device.unmapMemory(mem);
			}
		);
	}
};

struct CommandBuffer {
	vk::raii::CommandBuffer instance;
	CommandBuffer(const vk::raii::Device &device, const vk::raii::CommandPool &commandPool) : instance{nullptr} {
		vk::CommandBufferAllocateInfo commandBufferAllocateInfo{ *commandPool, vk::CommandBufferLevel::ePrimary, 1 };
		instance = std::move(device.allocateCommandBuffers(commandBufferAllocateInfo).front());
	}
	CommandBuffer &Record(std::function<void(const vk::raii::CommandBuffer &)> func, const vk::CommandBufferUsageFlags usage = vk::CommandBufferUsageFlagBits::eOneTimeSubmit) {
		vk::CommandBufferBeginInfo commandBufferBeginInfo{ usage };
		instance.begin(commandBufferBeginInfo);
		func(instance);
		instance.end();
		return *this;
	}
	void SubmitAndWait(const vk::raii::Queue &queue) {
		vk::SubmitInfo submitInfo{ nullptr, nullptr, *instance };
		queue.submit(submitInfo, nullptr);
		queue.waitIdle();
	}
	void Submit(const vk::raii::Queue &queue, const std::vector<vk::Semaphore>& waitSemaphores, const std::vector<vk::PipelineStageFlags> waitDstStageMask, const std::vector<vk::Semaphore>& signalSemaphore, const vk::raii::Fence &fence) {
		//static std::function<vk::Semaphore(const vk::raii::Semaphore&)> convertToVkSemaphore{ [](const vk::raii::Semaphore& semaphore) {
		//	return *semaphore;
		//}};
		//std::vector<vk::Semaphore> waitVkSemaphores{ waitSemaphores.size() }, signalVkSemaphores{ signalSemaphore.size() };
		//std::transform(waitSemaphores.begin(), waitSemaphores.end(), waitVkSemaphores.begin(), convertToVkSemaphore);
		//std::transform(signalSemaphore.begin(), signalSemaphore.end(), signalVkSemaphores.begin(), convertToVkSemaphore);
		vk::SubmitInfo submitInfo{ waitSemaphores.size(), waitSemaphores.data(), waitDstStageMask.size() == 0 ? nullptr : waitDstStageMask.data(), 1, &*instance, signalSemaphore.size(), signalSemaphore.data()};;
		queue.submit({ submitInfo }, *fence);
	}
	void Clear() {
		instance.reset();
	}
};

struct StagingBuffer {
	Buffer buffer;
	vk::Device device;
	vk::DeviceSize deviceSize;
	bool hasSubmitted;
	StagingBuffer(const vk::raii::PhysicalDevice &physicalDevice, const vk::raii::Device &device, const vk::DeviceSize deviceSize) :
		buffer{ physicalDevice, device, deviceSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible },
		device{ *device },
		deviceSize{ deviceSize },
		hasSubmitted{ false }
	{

	}

	~StagingBuffer() {
		if (!hasSubmitted) {
			LOG(WARNING) << "Staging buffer haven't been submitted. Is it intentional? Use StagingBuffer::NoSubmit() to disable this warning.";
		}
	}

	StagingBuffer &CopyToDevice(const void* data) {
		buffer.UploadData(data);

		return *this;
	}

	StagingBuffer &Submit(const vk::raii::CommandPool &commandPool, const vk::raii::Queue &queue, const vk::raii::Buffer &dst) {
		vk::CommandBufferAllocateInfo commandBufferAllocateInfo{ *commandPool, vk::CommandBufferLevel::ePrimary, 1 };
		vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(commandBufferAllocateInfo).front();

		vk::CommandBufferBeginInfo commandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		commandBuffer.begin(commandBufferBeginInfo);
		
		std::array<vk::BufferCopy, 1> regions = {
			vk::BufferCopy{0, 0, deviceSize}
		};

		commandBuffer.copyBuffer(*buffer.buffer, *dst, regions);

		commandBuffer.end();

		vk::SubmitInfo submitInfo{nullptr, nullptr, commandBuffer};
		queue.submit(submitInfo, nullptr);
		queue.waitIdle();

		device.freeCommandBuffers(*commandPool, 1, &commandBuffer);

		hasSubmitted = true;

		return *this;
	}

	StagingBuffer &NoSubmit() {
		hasSubmitted = true;

		return *this;
	}
};

class Image {
public:
	vk::raii::Image image;
private:
	vk::raii::DeviceMemory deviceMemory;
	const vk::raii::PhysicalDevice &physicalDevice;
	const vk::raii::Device &device;

	vk::ImageCreateInfo createInfo; //initialLayout reuse as the current layout
public:
	Image(const vk::raii::PhysicalDevice &physicalDevice, const vk::raii::Device &device, const vk::ImageCreateInfo imageInfo, const vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) : 
		image{device, imageInfo}, deviceMemory{ nullptr }, physicalDevice{physicalDevice}, device{ device }, createInfo{ imageInfo } {
		vk::MemoryRequirements memRequirement = image.getMemoryRequirements();
		vk::MemoryAllocateInfo memAlloc{ memRequirement.size, findMemoryType(physicalDevice.getMemoryProperties(), memRequirement.memoryTypeBits, properties) };
		deviceMemory = vk::raii::DeviceMemory{ device, memAlloc };

		image.bindMemory(*deviceMemory, 0);
	}

	void TransitionImageLayout(const vk::raii::CommandPool &commandPool, const vk::raii::Queue &queue, const vk::ImageLayout newLayout) {
		vk::ImageLayout &imageLayout = createInfo.initialLayout;
		CommandBuffer{ device, commandPool }.Record([&](const vk::raii::CommandBuffer &commandBuffer) {
			vk::AccessFlags srcAccessMask, dstAccessMask;
			vk::PipelineStageFlags sourceStage, destinationStage;
			vk::ImageSubresourceRange subresourceRange{ vk::ImageAspectFlagBits::eColor, 0, createInfo.mipLevels, 0, 1 };

			if (imageLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
				srcAccessMask = vk::AccessFlagBits::eNone;
				dstAccessMask = vk::AccessFlagBits::eTransferWrite;

				sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
				destinationStage = vk::PipelineStageFlagBits::eTransfer;
			} else if (imageLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferSrcOptimal) {
				srcAccessMask = vk::AccessFlagBits::eNone;
				dstAccessMask = vk::AccessFlagBits::eTransferRead;

				sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
				destinationStage = vk::PipelineStageFlagBits::eTransfer;
			} else if (imageLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
				srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				dstAccessMask = vk::AccessFlagBits::eShaderRead;

				sourceStage = vk::PipelineStageFlagBits::eTransfer;
				destinationStage = vk::PipelineStageFlagBits::eFragmentShader; // ？
			} else if ((imageLayout == vk::ImageLayout::eShaderReadOnlyOptimal || imageLayout == vk::ImageLayout::eUndefined) && newLayout == vk::ImageLayout::eGeneral) {
				srcAccessMask = vk::AccessFlagBits::eNone;
				dstAccessMask = vk::AccessFlagBits::eNone;

				sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
				destinationStage = vk::PipelineStageFlagBits::eComputeShader;
			} else if (imageLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
				srcAccessMask = vk::AccessFlagBits::eNone;
				dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

				sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
				destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
			} else {
				LOG(FATAL) << "Unsupported layout transition from " << vk::to_string(imageLayout) << " to " << vk::to_string(newLayout);
			}

			if (newLayout == vk::ImageLayout::eDepthAttachmentOptimal) {
				subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
				if (bool hasStencilComponent = (createInfo.format == vk::Format::eD32SfloatS8Uint || createInfo.format == vk::Format::eD24UnormS8Uint)) {
					subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
				}
			}

			vk::ImageMemoryBarrier barrier{ srcAccessMask, dstAccessMask, imageLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, *image, subresourceRange };

			commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);

		}).SubmitAndWait(queue);

		imageLayout = newLayout;
	}

	void UploadData(const vk::raii::CommandPool &commandPool, const vk::raii::Queue &queue, const vk::raii::Buffer &buffer) {
		vk::ImageLayout &imageLayout = createInfo.initialLayout;
		CommandBuffer{ device, commandPool }.Record([&](const vk::raii::CommandBuffer &commandBuffer) {
			vk::ImageSubresourceLayers subresource{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
			vk::BufferImageCopy region{ 0, 0, 0, subresource, vk::Offset3D{0, 0, 0}, createInfo.extent };
			commandBuffer.copyBufferToImage(*buffer, *image, imageLayout, region);
		}).SubmitAndWait(queue);
	}

	// via staging buffer
	void UploadData(const vk::raii::CommandPool &commandPool, const vk::raii::Queue &queue, void *data, vk::DeviceSize size) {
		Buffer stagingBuffer{ physicalDevice, device, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible };
		stagingBuffer.UploadData(data);

		UploadData(commandPool, queue, stagingBuffer.buffer);
	}

	void CopyToBuffer(const vk::raii::CommandPool &commandPool, const vk::raii::Queue &queue, const vk::raii::Buffer &buffer) {
		vk::ImageLayout &imageLayout = createInfo.initialLayout;
		CommandBuffer{ device, commandPool }.Record([&](const vk::raii::CommandBuffer &commandBuffer) {
			vk::ImageSubresourceLayers subresource{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
			vk::BufferImageCopy region{ 0, 0, 0, subresource, vk::Offset3D{0, 0, 0}, createInfo.extent };
			commandBuffer.copyImageToBuffer(*image, imageLayout, *buffer, region);
		}).SubmitAndWait(queue);
	}

	vk::raii::ImageView CreateDefaultImageView(vk::ImageAspectFlags aspectMask) {
		vk::ComponentMapping components{ vk::ComponentSwizzle::eIdentity,vk::ComponentSwizzle::eIdentity ,vk::ComponentSwizzle::eIdentity };
		vk::ImageSubresourceRange subresource{ aspectMask, 0, createInfo.mipLevels, 0, 1 };
		vk::ImageViewType type{};
		switch (createInfo.imageType) {
		case vk::ImageType::e1D:
			type = vk::ImageViewType::e1D;
			break;
		case vk::ImageType::e2D:
			type = vk::ImageViewType::e2D;
			break;
		case vk::ImageType::e3D:
			type = vk::ImageViewType::e3D;
			break;
		default:
			LOG(FATAL) << "Unknow image type " << vk::to_string(createInfo.imageType) << " to image view type.";
		}
		vk::ImageViewCreateInfo imageViewCreateInfo{ {}, *image, type, createInfo.format, components, subresource};
		return vk::raii::ImageView{ device, imageViewCreateInfo };
	}

	vk::ImageLayout GetImageLayout() const {
		return createInfo.initialLayout;
	}
};

struct Vertex {
	enum class AttributeFormat {
		Float = VK_FORMAT_R32_SFLOAT,
		Vec2 = VK_FORMAT_R32G32_SFLOAT,
		Vec3 = VK_FORMAT_R32G32B32_SFLOAT,
		Vec4 = VK_FORMAT_R32G32B32A32_SFLOAT,
		IVec2 = VK_FORMAT_R32G32_SINT,
		UVec4 = VK_FORMAT_R32G32B32A32_UINT,
		Double = VK_FORMAT_R64_SFLOAT
	};

	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	constexpr static vk::VertexInputBindingDescription getBindingDescription() {
		//Binding描述的是一个pack的大小
		vk::VertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = vk::VertexInputRate::eVertex;

		return bindingDescription;
	}

	static constexpr std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescription() {
		//Attribute描述的是一个pack中的布局
		std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

		constexpr uint32_t binding = getBindingDescription().binding;

		attributeDescriptions[0].binding = binding; // 第几个buffer
		attributeDescriptions[0].location = 0; // 处于shader的哪个location
		attributeDescriptions[0].format = (vk::Format)AttributeFormat::Vec3;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = binding;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = (vk::Format)AttributeFormat::Vec3;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = binding;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = (vk::Format)AttributeFormat::Vec2;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	bool operator==(const Vertex &other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}

};

template<> struct std::hash<Vertex> {
	std::size_t operator()(Vertex const &vertex) const {
		return ((std::hash<glm::vec3>()(vertex.pos) ^ (std::hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (std::hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};

struct Model {
	using Vertex = Vertex;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	Model(std::string filename) {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		if (std::string warn, err; !tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
			LOG(ERROR) << "Cannot load model: " << filename << std::endl << "\tMessage(Warn): " << warn << std::endl << "\tMessage(Err): " << err;
		}

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		for (const tinyobj::shape_t &shape : shapes) {
			for (const tinyobj::index_t &index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Vulkan的Y轴是从上往下，tinyObj的Y轴是从下往上
				};

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = vertices.size();
					vertices.push_back(vertex);
				}

				indices.push_back(uniqueVertices[vertex]);
			}
		}

		LOG(INFO) << "Model Info: Vertex=" << vertices.size() << ", Index=" << indices.size();
	}
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

void getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::copy(glfwExtensions, glfwExtensions + glfwExtensionCount, std::back_inserter(extensionLayers));

	std::cout << "GLFW extensions: ";
	std::copy(glfwExtensions, glfwExtensions + glfwExtensionCount, std::ostream_iterator<const char *>(std::cout, ", "));
	std::cout << std::endl;
}

int run2(int argc, char* argv[]) {
#pragma region Create device
	const vk::ApplicationInfo appInfo{ "vulkan test", VK_MAKE_VERSION(1, 0, 0), "no engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0 };
	const vk::InstanceCreateInfo instanceCreateInfo{ {}, &appInfo, validationLayers.size(), validationLayers.data(), extensionLayers.size(), extensionLayers.data() };

	const vk::raii::Context context;
	const vk::raii::Instance instance{ context, instanceCreateInfo };

	const vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	const vk::DebugUtilsMessageTypeFlagsEXT    messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	const vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, &debugCallback);
	const vk::raii::DebugUtilsMessengerEXT debugMessenger{ instance, debugUtilsMessengerCreateInfoEXT };

	const vk::raii::PhysicalDevice physicalDevice = vk::raii::PhysicalDevices{ instance }.front();
	LOG(INFO) << "Using device: " << physicalDevice.getProperties().deviceName;

	std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
	auto queuePropIter = std::find_if(queueProps.begin(), queueProps.end(), [](const vk::QueueFamilyProperties& prop) {
		return (prop.queueFlags & vk::QueueFlagBits::eCompute);
	});
	CHECK(queuePropIter != queueProps.end()) << "Can't find compute queue.";
	std::ptrdiff_t computeQueueFamilyIndex = std::distance(queueProps.begin(), queuePropIter);

	
	float queuePriority = 0;
	std::array<vk::DeviceQueueCreateInfo, 1> queueCreateInfo{
		vk::DeviceQueueCreateInfo{{}, computeQueueFamilyIndex, 1, &queuePriority },
	};

	vk::DeviceCreateInfo deviceCreateInfo{ {}, queueCreateInfo.size(), queueCreateInfo.data() };
	vk::raii::Device device{ physicalDevice, deviceCreateInfo };

	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
#pragma endregion

	vk::raii::Queue computeQueue{ device, computeQueueFamilyIndex, 0 };

	vk::CommandPoolCreateInfo commandPoolCreateInfo{ {}, computeQueueFamilyIndex };
	vk::raii::CommandPool commandPool{device, commandPoolCreateInfo};

#pragma region Input data
	/* ================ Buffer data =================== */
	constexpr size_t BUFFER_ELEMENTS = 200;
	// vector{num} 是设置数组的内容， vector(num)是设置数组的长度
	std::vector<uint32_t> input(BUFFER_ELEMENTS);
	std::iota(input.begin(), input.end(), 0);

	std::cout << "Input data: "; 
	std::copy(input.begin(), input.end(), std::ostream_iterator<uint32_t>(std::cout, ", "));
	std::cout << std::endl;


	vk::DeviceSize bufferDeviceSize{ sizeof(input[0]) * input.size() };
	Buffer buffer{ physicalDevice, device, bufferDeviceSize, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible };
	buffer.UploadData(input.data());

	/* =================== Image ================== 640x480 */
	cv::Mat mat = cv::imread("D:\\SRCCODES\\rgbd_dataset_freiburg1_desk\\rgb\\1305031473.127744.png", cv::ImreadModes::IMREAD_COLOR);
	CHECK(!mat.empty()) << "Can't read image!";
	cv::cvtColor(mat, mat, cv::ColorConversionCodes::COLOR_BGR2RGBA);

	vk::ImageCreateInfo imageCreateInfo{ {}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Uint, vk::Extent3D{mat.cols, mat.rows, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined };
	vk::DeviceSize imageBufferDeviceSize{ mat.cols * mat.rows * mat.channels() * sizeof(uchar) };
	Image image{ physicalDevice, device, imageCreateInfo };
	image.TransitionImageLayout(commandPool, computeQueue, vk::ImageLayout::eTransferDstOptimal);
	image.UploadData(commandPool, computeQueue, mat.data, imageBufferDeviceSize);
	image.TransitionImageLayout(commandPool, computeQueue, vk::ImageLayout::eShaderReadOnlyOptimal);
	image.TransitionImageLayout(commandPool, computeQueue, vk::ImageLayout::eGeneral);
	vk::raii::ImageView imageView = image.CreateDefaultImageView(vk::ImageAspectFlagBits::eColor);

	vk::ImageCreateInfo outputImageCreateInfo{ {}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Uint, vk::Extent3D{mat.cols, mat.rows, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage, vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined };
	Image outputImage{ physicalDevice, device, outputImageCreateInfo };
	//outputImage.TransitionImageLayout(commandPool, computeQueue, vk::ImageLayout::eTransferSrcOptimal);
	outputImage.TransitionImageLayout(commandPool, computeQueue, vk::ImageLayout::eGeneral);
	vk::raii::ImageView outputImageView = outputImage.CreateDefaultImageView(vk::ImageAspectFlagBits::eColor);

	vk::SamplerCreateInfo samplerCreateInfo{ {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, 0.0f, false, 16, false, vk::CompareOp::eNever, 0.0f, 0.0f, vk::BorderColor::eIntOpaqueBlack, false };
	vk::raii::Sampler sampler{ device, samplerCreateInfo };
#pragma endregion

#pragma region Create pipeline 
	constexpr bool useStorageImage = true;
	constexpr bool useSSBO = !useStorageImage;
	std::array<vk::DescriptorSetLayoutBinding, (useStorageImage ? 2 : 1)> layoutBinding;
	if constexpr (useStorageImage) {
		layoutBinding[0] = vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute, nullptr };
		layoutBinding[1] = vk::DescriptorSetLayoutBinding{ 2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute, nullptr };
	} else {
		layoutBinding[0] = vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr };
	}

	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{ {}, layoutBinding.size(), layoutBinding.data() };
	vk::raii::DescriptorSetLayout descriptorSetLayout{ device, descriptorSetLayoutCreateInfo };

	std::array<vk::DescriptorPoolSize, 1> poolSize;
	if constexpr (useStorageImage) {
		poolSize[0] = vk::DescriptorPoolSize{ vk::DescriptorType::eStorageImage, 2 };
	} else {
		poolSize[0] = vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBuffer, 1 };
	}
	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSize.size(), poolSize.data() };
	vk::raii::DescriptorPool descriptorPool{ device, descriptorPoolCreateInfo };

	vk::DescriptorSetAllocateInfo descriptorSetAllocationInfo{ *descriptorPool, 1, &*descriptorSetLayout };
	vk::raii::DescriptorSets descriptorSets{ device, descriptorSetAllocationInfo };

	// 配置DescriptorSets
	if constexpr (useStorageImage) { 
		vk::DescriptorImageInfo storageImageDescriptorImageInfo{ *sampler, *imageView, image.GetImageLayout()};
		vk::DescriptorImageInfo storageOutputImageDescriptorImageInfo{ *sampler, *outputImageView, outputImage.GetImageLayout() };
		device.updateDescriptorSets({
			vk::WriteDescriptorSet{*descriptorSets[0], layoutBinding[0].binding, 0,  1, vk::DescriptorType::eStorageImage, &storageImageDescriptorImageInfo},
			vk::WriteDescriptorSet{*descriptorSets[0], layoutBinding[1].binding, 0,  1, vk::DescriptorType::eStorageImage, &storageOutputImageDescriptorImageInfo}
		}, nullptr);
	} else {
		vk::DescriptorBufferInfo storageBufferInfo{ *buffer.buffer, 0, bufferDeviceSize };
		device.updateDescriptorSets({
			vk::WriteDescriptorSet{*descriptorSets[0], layoutBinding[0].binding, 0, 1, layoutBinding[0].descriptorType, nullptr, &storageBufferInfo}
		}, nullptr);
	}

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{ {}, *descriptorSetLayout };
	vk::raii::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

	std::vector<char> shaderCode = readBinary("imgcomp.spv");
	vk::ShaderModuleCreateInfo shaderModuleCreateInfo{ {}, shaderCode.size(), (uint32_t*)shaderCode.data() };
	vk::raii::ShaderModule shaderModule = device.createShaderModule(shaderModuleCreateInfo);
	vk::PipelineShaderStageCreateInfo shaderStageInfo{ {}, vk::ShaderStageFlagBits::eCompute, *shaderModule, "main", nullptr };

	vk::ComputePipelineCreateInfo pipelineCreateInfo{ {}, shaderStageInfo, *pipelineLayout };
	vk::raii::Pipeline pipeline = device.createComputePipeline(nullptr, pipelineCreateInfo);

#pragma endregion
	
#pragma region Run compute shader
	CommandBuffer{ device, commandPool }.Record([&](const vk::raii::CommandBuffer &commandBuffer) {
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, { *descriptorSets[0] }, nullptr);

		commandBuffer.dispatch(640 / 10, 480 / 10, 1); // localsize = 10, 10, 1
	}).SubmitAndWait(computeQueue);
	//vk::CommandBufferAllocateInfo commandBufferAllocateInfo{ *commandPool, vk::CommandBufferLevel::ePrimary, 1 };
	//vk::raii::CommandBuffers commandBuffers{ device, commandBufferAllocateInfo };

	//vk::raii::CommandBuffer &commandBuffer = commandBuffers[0];
	//vk::CommandBufferBeginInfo commandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	//commandBuffer.begin(commandBufferBeginInfo);
	//commandBuffer.end();

	//vk::FenceCreateInfo fenceCreateInfo{};
	//vk::raii::Fence fence{ device, fenceCreateInfo };

	//vk::SubmitInfo submitInfo{ nullptr, nullptr, *commandBuffer };
	//computeQueue.submit(submitInfo, *fence);
	//while (device.waitForFences({ *fence }, VK_TRUE, FenceTimeout) == vk::Result::eTimeout);
#pragma endregion

	if constexpr (useSSBO) {
		auto output = buffer.MapFromDevice<uint32_t>();

		std::cout << "Output data: ";
		for (size_t i = 0; i < input.size(); ++i) {
			std::cout << output[i] << ", ";
		}
		std::cout << std::endl;
	} else {
		Buffer result{ physicalDevice, device, imageBufferDeviceSize, vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible };
		outputImage.CopyToBuffer(commandPool, computeQueue, result.buffer);

		auto output = result.MapFromDevice<uchar>();
		cv::Mat outputMat(mat.cols, mat.rows, CV_8UC4, output.get());
		cv::imshow("result", outputMat);
		cv::waitKey();
		cv::destroyAllWindows();
	}

	return 0;
}


namespace TemplateDescriptorPoolSizeImpl {
	//template <size_t T, size_t N, size_t ...I>
	//constexpr size_t calDescriptorCountsImpl(const std::array<vk::DescriptorSetLayoutBinding, N> &arr, std::index_sequence<I...>) {
	//	static_assert(sizeof...(I) == N, "first index_sequence should have N elements as same as arr.");
	//	return (... + (static_cast<size_t>(arr[I].descriptorType) == T ? arr[I].descriptorCount : 0));
	//}

	template <size_t T, const auto &A, size_t ...I>
	constexpr size_t calDescriptorCountsImpl(std::index_sequence<I...>) {
		static_assert(sizeof...(I) == std::size(A), "first index_sequence should have N elements as same as arr.");
		return (... + (static_cast<size_t>(A[I].descriptorType) == T ? A[I].descriptorCount : 0));
	}

	//对某个type进行统计
	template <size_t T, const auto &A>
	constexpr size_t calDescriptorCountsImpl() {
		return calDescriptorCountsImpl<T, A>(std::make_index_sequence<std::size(A)>());
	}

	//template <size_t N, size_t ...I, size_t ...R>
	//constexpr auto calDescriptorCounts(const std::array<vk::DescriptorSetLayoutBinding, N> &arr, std::index_sequence<I...> arrIdx, std::index_sequence<R...>) {
	//	static_assert(sizeof...(I) == N, "first index_sequence should have N elements as same as arr.");
	//	static_assert(sizeof...(R) == 11, "second index_sequence should have 11 elements from 0 to 10.");
	//	//return (... + arr[I].descriptorCount);
	//	return std::array{ calDescriptorCountsImpl<R>(arr, arrIdx)... };
	//	//return std::index_sequence< calDescriptorCountsImpl<R>(arr, arrIdx)... >();
	//}

	constexpr size_t DescriptorTypeCount = 11;

	template <const auto &A, size_t ...R>
	constexpr auto calDescriptorCounts(std::index_sequence<R...>) {
		static_assert(sizeof...(R) == DescriptorTypeCount, "second index_sequence should have `DescriptorTypeCount` elements.");
		return std::index_sequence< calDescriptorCountsImpl<R, A>()... >();
	}


	// 对descriptorBinding进行统计
	template <const auto &A>
	constexpr auto calDescriptorCounts() {
		return calDescriptorCounts<A>(std::make_index_sequence<DescriptorTypeCount>());
	}

	template <size_t ...A, size_t ...N>
	constexpr auto calcDescriptorPoolSizeImpl(std::index_sequence<A...>, std::index_sequence<N...>) {
		return std::array{
			vk::DescriptorPoolSize{ static_cast<vk::DescriptorType>(N), getFromTemplate<N, A...>::value }...
		};
	}

	//void Test() {
//	constexpr auto test = std::integer_sequence<size_t, 4, 0, 0, 11, 0, 23, 0, 4>();
//	constexpr auto d = pickNonZeroElement(test);
//
//	constexpr static std::array<vk::DescriptorSetLayoutBinding, 1> descriptorSetLayoutBindings{
//		//这里的binding指的是uniform的binding，不是location的binding，不一样的，同样是descriptor，这个descriptor是属于pipelinelayout的，种类不一样，因此binding的点可以一样不冲突（使用set区分，详看pipelinelayout的笔记）
//		vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 3, vk::ShaderStageFlagBits::eVertex}
//	};
//
//	constexpr static std::array<vk::DescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings2{
//		vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 2, vk::ShaderStageFlagBits::eVertex},
//		vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eInputAttachment, 2, vk::ShaderStageFlagBits::eVertex}
//	};
//
//	// 0-10 type
//
//	constexpr auto range = std::make_index_sequence<11>();
//
//	constexpr auto toarr = to_array(range);
//
//	//constexpr auto push = pushFrontElement<5>(range);
//
//	constexpr auto q = calDescriptorCountsImpl<6, descriptorSetLayoutBindings2>();
//
//	constexpr auto x1 = calDescriptorCounts<descriptorSetLayoutBindings>();
//	constexpr auto x2 = calDescriptorCounts<descriptorSetLayoutBindings2>();
//
//	constexpr auto m = merge_index_sequence(x1, x2);
//	constexpr static auto arrTypes = to_array(m);
//	constexpr auto idsx = to_index_seq<arrTypes>();
//
//	constexpr auto uniqueTypes = pickNonZeroElement(m);
//
//	constexpr auto ret = calcDescriptorPoolSizeImpl(m, uniqueTypes);
//	//constexpr auto ret = calcDescriptorPoolSizeImpl<arrTypes>(uniqueTypes);
//
//	constexpr auto p = calcDescriptorPoolSize<descriptorSetLayoutBindings, descriptorSetLayoutBindings2>();
//
//}

}

/**
* @brief Each parameter must be a constexpr array to std::array<vk::DescriptorSetLayoutBinding, N>
**/
template <const auto &...A>
constexpr auto calcDescriptorPoolSize() {
	constexpr auto types = merge_index_sequence(TemplateDescriptorPoolSizeImpl::calDescriptorCounts<A>()...);
	constexpr auto uniqueTypes = pickNonZeroElement(types);

	return TemplateDescriptorPoolSizeImpl::calcDescriptorPoolSizeImpl(types, uniqueTypes);
}




int run(int argc, char *argv[]) {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	const size_t WIDTH = 800, HEIGHT = 600;

	GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

#pragma region Create device & Queues
	getRequiredExtensions();
	const vk::ApplicationInfo appInfo{ "vulkan test", VK_MAKE_VERSION(1, 0, 0), "no engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_1 };
	const vk::InstanceCreateInfo instanceCreateInfo{ {}, &appInfo, validationLayers.size(), validationLayers.data(), extensionLayers.size(), extensionLayers.data() };

	const vk::raii::Context context;
	const vk::raii::Instance instance{ context, instanceCreateInfo };

	const vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	const vk::DebugUtilsMessageTypeFlagsEXT    messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	const vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, &debugCallback);
	const vk::raii::DebugUtilsMessengerEXT debugMessenger{ instance, debugUtilsMessengerCreateInfoEXT };

	vk::raii::SurfaceKHR surface{ nullptr };
	{
		VkSurfaceKHR vksurface;
		CHECK(glfwCreateWindowSurface(*instance, window, nullptr, &vksurface) == VK_SUCCESS) << "failed to create window surface!";
		surface = vk::raii::SurfaceKHR{ instance, vksurface };
	}
	defer(glfwDestroyWindow(window); glfwTerminate());

	const vk::raii::PhysicalDevice physicalDevice = vk::raii::PhysicalDevices{ instance }.front();
	LOG(INFO) << "Using device: " << physicalDevice.getProperties().deviceName;

	std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
	auto queuePropIter = std::find_if(queueProps.begin(), queueProps.end(), [](const vk::QueueFamilyProperties &prop) {
		return (prop.queueFlags & vk::QueueFlagBits::eGraphics);
	});
	CHECK(queuePropIter != queueProps.end()) << "Can't find graphic queue.";
	int graphicQueueFamilyIndex = std::distance(queueProps.begin(), queuePropIter);

	int presentQueueFamilyIndex = -1;
	for (size_t i = 0; i < queueProps.size(); ++i) {
		if (physicalDevice.getSurfaceSupportKHR(i, *surface)) {
			presentQueueFamilyIndex = i;
			break;
		}
	}
	CHECK(presentQueueFamilyIndex != -1) << "Cannot find present queue.";

	std::set<int> uniqueQueueFamilyIndices{ graphicQueueFamilyIndex, presentQueueFamilyIndex };

	float queuePriority = 0;
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfo;
	for (const int queue : uniqueQueueFamilyIndices) {
		queueCreateInfo.push_back(vk::DeviceQueueCreateInfo{ {}, queue, 1, &queuePriority });
	}

	constexpr static std::array<const char *, 2> deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
	};
	constexpr static std::array<const char *, 1> validationLayers{
		"VK_LAYER_KHRONOS_validation"
	};
	vk::PhysicalDeviceFeatures deviceFeatures;
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;

	vk::DeviceCreateInfo deviceCreateInfo{ {}, queueCreateInfo.size(), queueCreateInfo.data(), validationLayers.size(), validationLayers.data(), deviceExtensions.size(), deviceExtensions.data(), &deviceFeatures};
	vk::raii::Device device{ physicalDevice, deviceCreateInfo };

	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
	vk::raii::Queue graphicQueue{ device, graphicQueueFamilyIndex, 0 };
	vk::raii::Queue presentQueue{ device, presentQueueFamilyIndex, 0 };
#pragma endregion

#pragma region Swap chain & Frame buffers
	struct SwapChainSupportDetails {
		vk::SurfaceCapabilitiesKHR capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> presentModes;
	} details{
		physicalDevice.getSurfaceCapabilitiesKHR(*surface),
		physicalDevice.getSurfaceFormatsKHR(*surface),
		physicalDevice.getSurfacePresentModesKHR(*surface)
	};

	struct SwapChainData {
		vk::Extent2D extent;
		vk::SurfaceFormatKHR format;
		vk::PresentModeKHR present;
		vk::SharingMode sharingMode;
		uint32_t queueFamilyIndexCount;
		uint32_t *queueFamilyIndices;

	} swapChainData{
		[&window](const vk::SurfaceCapabilitiesKHR capabilities) {
			if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
				return capabilities.currentExtent;
			}
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			return vk::Extent2D{
				std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
				std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
			};
		}(details.capabilities),
		[&details]() {
			auto found = std::find_if(details.formats.begin(), details.formats.end(), [](const vk::SurfaceFormatKHR &availableFormat) {
			return availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			});
			if (found == details.formats.end()) return details.formats[0];
			return *found;
		}(),
		[&details]() {
			auto found = std::find_if(details.presentModes.begin(), details.presentModes.end(), [](const vk::PresentModeKHR &availablePresentMode) {
				return availablePresentMode == vk::PresentModeKHR::eMailbox;
			});
			if (found == details.presentModes.end()) return vk::PresentModeKHR::eFifo;
			return *found;
		}(),
	};

	//CHECK(uniqueQueueFamilyIndices.size() == 1) << "graphic queue and present queue is not the same one";
	
	//vk::SharingMode swapChainSharingMode = uniqueQueueFamilyIndices.size() == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
	std::vector<uint32_t> queueFamilyIndices{ (uint32_t)graphicQueueFamilyIndex, (uint32_t)presentQueueFamilyIndex };
	if (graphicQueueFamilyIndex == presentQueueFamilyIndex) {
		swapChainData.sharingMode = vk::SharingMode::eExclusive;
		swapChainData.queueFamilyIndexCount = 0;
		swapChainData.queueFamilyIndices = nullptr;
	} else {
		swapChainData.sharingMode = vk::SharingMode::eConcurrent;
		swapChainData.queueFamilyIndexCount = 2;
		swapChainData.queueFamilyIndices = queueFamilyIndices.data();
	}
	//if (uniqueQueueFamilyIndices.size() == 1) queueFamilyIndices.clear(); //FIXME: details.capabilities.maxImageCount可能为-1
	vk::SwapchainCreateInfoKHR swapChainCreateInfo{ {}, *surface, std::min(details.capabilities.minImageCount + 1, details.capabilities.maxImageCount), swapChainData.format.format, swapChainData.format.colorSpace, swapChainData.extent, 1, vk::ImageUsageFlagBits::eColorAttachment, swapChainData.sharingMode, swapChainData.queueFamilyIndexCount, swapChainData.queueFamilyIndices, details.capabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, swapChainData.present, true, nullptr};
	vk::raii::SwapchainKHR swapChain{ device, swapChainCreateInfo };

	std::vector<vk::Image> swapChainImages = swapChain.getImages();

	std::vector<vk::raii::ImageView> swapChainImageViews;
	std::transform(swapChainImages.begin(), swapChainImages.end(), std::back_inserter(swapChainImageViews), [&swapChainData, &device](const vk::Image &img) {
		vk::ImageSubresourceRange subresource{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		vk::ImageViewCreateInfo imageViewCreateInfo{ {}, img, vk::ImageViewType::e2D, swapChainData.format.format, {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity}, subresource };
		return vk::raii::ImageView{ device, imageViewCreateInfo };
	});
	LOG(INFO) << "Swap chain size: " << swapChainImageViews.size();

#pragma endregion

#pragma region Render pass
	std::array<vk::Format, 3> candidatesForDepthFormat = { vk::Format::eD32Sfloat,  vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint };
	vk::Format depthFormat = *std::find_if(candidatesForDepthFormat.begin(), candidatesForDepthFormat.end(), [&physicalDevice](const vk::Format format) {
		vk::FormatProperties props = physicalDevice.getFormatProperties(format);
		return (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment);
	});
	std::array<vk::AttachmentDescription, 1> attachments{
		//vk::AttachmentDescription{{}, depthFormat, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal},
		vk::AttachmentDescription{{}, swapChainData.format.format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR},
	};
	std::array<vk::AttachmentReference, attachments.size()> attachmentRefs{
		//vk::AttachmentReference{0, vk::ImageLayout::eDepthStencilAttachmentOptimal},
		vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal}
	};
	std::array<vk::SubpassDescription, 1> subpass{
		vk::SubpassDescription{{}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, attachmentRefs.data(), nullptr, nullptr, 0, nullptr},
	};
	std::array<vk::SubpassDependency, 1> subpassDependency{
		vk::SubpassDependency{VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite, {}},
	};
	
	vk::RenderPassCreateInfo renderPassCreateInfo{ {}, attachments.size(), attachments.data(), subpass.size(), subpass.data(), subpassDependency.size(), subpassDependency.data() };;
	vk::raii::RenderPass renderPass{ device, renderPassCreateInfo };
#pragma endregion

#pragma region FrameBuffers
	//vk::ImageCreateInfo colorImageCreateInfo{ {}, vk::ImageType::e2D, swapChainData.format.format, vk::Extent3D{ swapChainData.extent, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined };
	//Image colorImage{ physicalDevice, device, colorImageCreateInfo };
	//vk::raii::ImageView colorImageView = colorImage.CreateDefaultImageView(vk::ImageAspectFlagBits::eColor);
	//vk::ImageCreateInfo depthImageCreateInfo{ {}, vk::ImageType::e2D, depthFormat, vk::Extent3D{ swapChainData.extent, 1 }, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined };
	//Image depthImage{ physicalDevice, device, depthImageCreateInfo };
	//vk::raii::ImageView depthImageView = depthImage.CreateDefaultImageView(vk::ImageAspectFlagBits::eDepth);

	std::vector<vk::raii::Framebuffer> frameBuffers;
	std::transform(swapChainImageViews.begin(), swapChainImageViews.end(), std::back_inserter(frameBuffers), [&renderPass, &swapChainData, &device](const vk::raii::ImageView& swapChainImageView) {
		std::array<vk::ImageView, 1> attachments = { *swapChainImageView };
		
		vk::FramebufferCreateInfo createInfo{ {}, *renderPass, attachments.size(), attachments.data(), swapChainData.extent.width, swapChainData.extent.height, 1 };
		return vk::raii::Framebuffer{ device, createInfo };
	});
#pragma endregion

	vk::CommandPoolCreateInfo commandPoolCreateInfo{ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicQueueFamilyIndex };
	vk::raii::CommandPool commandPool{ device, commandPoolCreateInfo };
	CommandBuffer commandBuffer{ device, commandPool };

#pragma region Buffers
	Model model{ "models\\viking_room.obj" };
	Buffer vertexBuffer{ physicalDevice, device, model.vertices.size() * sizeof(model.vertices[0]), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal };
	//vertexBuffer.UploadData(model.vertices.data());
	StagingBuffer{ physicalDevice, device, vertexBuffer.deviceSize }.CopyToDevice(model.vertices.data()).Submit(commandPool, graphicQueue, vertexBuffer.buffer);
	Buffer indexBuffer{ physicalDevice, device, model.indices.size() * sizeof(model.indices[0]), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal };
	//indexBuffer.UploadData(model.indices.data());
	StagingBuffer{ physicalDevice, device, indexBuffer.deviceSize }.CopyToDevice(model.vertices.data()).Submit(commandPool, graphicQueue, indexBuffer.buffer);
	Buffer uniformBuffer{ physicalDevice, device, sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible };
	auto uniformBufferData = uniformBuffer.MapFromDevice<UniformBufferObject>();
#pragma endregion

#pragma region Descriptor set
	//PipelineLayout相当于给shader（看成一个函数）一个即时的参数，由descriptorSet提供参数的内容，descriptorSetLayout提供参数的签名
	constexpr static std::array<vk::DescriptorSetLayoutBinding, 1> descriptorSetLayoutBindings{
		//这里的binding指的是uniform的binding，不是location的binding，不一样的，同样是descriptor，这个descriptor是属于pipelinelayout的，种类不一样，因此binding的点可以一样不冲突（使用set区分，详看pipelinelayout的笔记）
		vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}
	};

	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{ {}, descriptorSetLayoutBindings.size(), descriptorSetLayoutBindings.data() };
	vk::raii::DescriptorSetLayout descriptorSetLayout{ device, descriptorSetLayoutCreateInfo };

	//std::array<vk::DescriptorPoolSize, 1> descriptorPoolSize{
	//	vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1 },
	//};
	constexpr auto descriptorPoolSize = calcDescriptorPoolSize<descriptorSetLayoutBindings>();
	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, descriptorPoolSize.size(), descriptorPoolSize.data() };
	vk::raii::DescriptorPool descriptorPool{ device, descriptorPoolCreateInfo };

	//可以创建多个descriptorSets，每个元素的索引作为shader中的set
	vk::DescriptorSetAllocateInfo descriptorSetsCreateInfo{ *descriptorPool, 1, &*descriptorSetLayout };
	vk::raii::DescriptorSet descriptorSet = std::move(vk::raii::DescriptorSets{ device, descriptorSetsCreateInfo }.front());

	vk::DescriptorBufferInfo descriptorBufferInfo{ *uniformBuffer.buffer, 0, uniformBuffer.deviceSize };
	vk::WriteDescriptorSet uniformDescriptorSet{ *descriptorSet, descriptorSetLayoutBindings[0].binding, 0, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfo, nullptr };
	device.updateDescriptorSets({ uniformDescriptorSet }, nullptr);
#pragma endregion

#pragma region Graphic Pipeline
	std::vector<char> vertShader = readBinary("vertex.spv"), fragShader = readBinary("frag.spv");
	vk::raii::ShaderModule vertexModule{ device, vk::ShaderModuleCreateInfo{ {}, vertShader.size(), (uint32_t*)vertShader.data() }};
	vk::raii::ShaderModule fragModule{ device, vk::ShaderModuleCreateInfo{ {}, fragShader.size(), (uint32_t *)fragShader.data() } };

	constexpr vk::VertexInputBindingDescription bindingDescription = Model::Vertex::getBindingDescription();
	constexpr auto attributeDescription = Model::Vertex::getAttributeDescription();
	constexpr vk::PipelineColorBlendAttachmentState colorBlendAttachment{
		false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};
	vk::Viewport viewport{ 0.0f, 0.0f, (float)swapChainData.extent.width, (float)swapChainData.extent.height, 0.0f, 1.0f }; /* 视窗 */
	vk::Rect2D scissor{ {0, 0}, swapChainData.extent }; /* 裁剪 */
	constexpr static std::array<vk::DynamicState, 2> dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	struct PipelineData {
		std::array<vk::PipelineShaderStageCreateInfo, 2> ShaderStageCreateInfo;
		vk::PipelineVertexInputStateCreateInfo VertexInputStateCreateInfo;
		vk::PipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo;
		vk::PipelineViewportStateCreateInfo ViewportStateCreateInfo;
		vk::PipelineRasterizationStateCreateInfo RasterizationStateCreateInfo;
		vk::PipelineMultisampleStateCreateInfo MultisampleStateCreateInfo;
		vk::PipelineDepthStencilStateCreateInfo DepthStencilStateCreateInfo;
		vk::PipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo;
		vk::PipelineDynamicStateCreateInfo DynamicStateCreateInfo;
	} pipelineData{
		std::array<vk::PipelineShaderStageCreateInfo, pipelineData.ShaderStageCreateInfo.size()> {
			vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eVertex, *vertexModule, "main", nullptr},
			vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, *fragModule, "main", nullptr},
		},
		vk::PipelineVertexInputStateCreateInfo {
			{}, 1, &bindingDescription, attributeDescription.size(), attributeDescription.data()
		},
		vk::PipelineInputAssemblyStateCreateInfo {
			{}, vk::PrimitiveTopology::eTriangleList, false
		},
		vk::PipelineViewportStateCreateInfo {
			{}, 1, &viewport, 1, &scissor
		},
		vk::PipelineRasterizationStateCreateInfo {
			{}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, false,
			0.f, 0.f, 0.f, 1.f
		},
		vk::PipelineMultisampleStateCreateInfo {
			{}, vk::SampleCountFlagBits::e1, false, {}, nullptr, false, false
		},
		vk::PipelineDepthStencilStateCreateInfo {
			{}, true, true, vk::CompareOp::eLess, false, false, {}, {}, 0.f, 1.f
		},
		vk::PipelineColorBlendStateCreateInfo {
			{}, false, vk::LogicOp::eCopy, 1, &colorBlendAttachment, std::array<float, 4> { 0.f, 0.f, 0.f, 0.f }
		},
		vk::PipelineDynamicStateCreateInfo {
			{}, dynamicStates.size(), dynamicStates.data()
		},
	};

	//这里可以传多个descriptorSetLayout，相当于可以有多个函数签名（如果将shader看成函数）
	//这里descriptorSetLayout为一个数组，数组元素的索引就是shader里指定的set，比如:
	// layout(set = 1, binding = 0) uniform X { ...
	// 指的是，使用descriptorSetLayout[1]中的binding=0的元素（由对应的descriptorSet
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{ {}, 1, &*descriptorSetLayout, 0, nullptr };
	vk::raii::PipelineLayout pipelineLayout{ device, pipelineLayoutCreateInfo };

	vk::GraphicsPipelineCreateInfo pipelineCreateInfo{ {}, pipelineData.ShaderStageCreateInfo.size(),
		pipelineData.ShaderStageCreateInfo.data(),
		&pipelineData.VertexInputStateCreateInfo,
		&pipelineData.InputAssemblyStateCreateInfo,
		nullptr,
		&pipelineData.ViewportStateCreateInfo,
		&pipelineData.RasterizationStateCreateInfo,
		&pipelineData.MultisampleStateCreateInfo,
		&pipelineData.DepthStencilStateCreateInfo,
		&pipelineData.ColorBlendStateCreateInfo,
		&pipelineData.DynamicStateCreateInfo,
		*pipelineLayout,
		*renderPass
	};
	vk::raii::Pipeline pipeline = std::move(vk::raii::Pipelines{ device, nullptr, { pipelineCreateInfo } }.front());
#pragma endregion

	constexpr static vk::SemaphoreCreateInfo semaphoreCreateInfo{};
	vk::raii::Semaphore imageAvailableSemaphores{ device, semaphoreCreateInfo }, renderFinishedSemaphores{ device, semaphoreCreateInfo };
	vk::raii::Fence fence{ device, vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled} };
	
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		constexpr uint64_t uint64_max = std::numeric_limits<uint64_t>::max();
		device.waitForFences({ *fence }, true, uint64_max);
		device.resetFences({ *fence });

		//std::pair<vk::Result, uint32_t> nextImage = device.acquireNextImage2KHR(vk::AcquireNextImageInfoKHR{*swapChain, std::numeric_limits<uint64_t>::max(), *imageAvailableSemaphores, nullptr});
		uint32_t imageIndex;
		vk::Result result;
		std::tie(result, imageIndex) = swapChain.acquireNextImage(uint64_max, *imageAvailableSemaphores);
		//VkResult result = vkAcquireNextImageKHR(*device, *swapChain, std::numeric_limits<uint64_t>::max(), *imageAvailableSemaphores, VK_NULL_HANDLE, &imageIndex);
		if (result == vk::Result::eErrorOutOfDateKHR) {
			LOG(WARNING) << "recreate swap chain";
			continue;
		}
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
			LOG(FATAL) << "failed to acquire swap chain image!";
		}

		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject &ubo = uniformBufferData[0];
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), swapChainData.extent.width / (float)swapChainData.extent.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1; //在OpenGL是正确的，但是在vulkan是反的，需要乘上-1反回去

		std::array<vk::ClearValue, 2> clearValues{};
		vk::RenderPassBeginInfo renderpassInfo{ *renderPass, *frameBuffers[0], vk::Rect2D{vk::Offset2D{0,0}, swapChainData.extent}, clearValues.size(), clearValues.data() };
		std::array<vk::Buffer, 1> vertexBuffers{ *vertexBuffer.buffer };
		std::array<vk::DeviceSize, 1> offsets{ 0 };
		vk::Viewport viewport{ 0.0f, 0.0f, (float)swapChainData.extent.width, (float)swapChainData.extent.height, 0.0f, 1.0f };
		vk::Rect2D scissor{ {0, 0}, swapChainData.extent };
		commandBuffer.Clear();
		//vk::CommandBufferBeginInfo commandBufferBeginInfo{ };
		//commandBuffer.instance.begin(commandBufferBeginInfo);
		//vk::raii::CommandBuffer &cmd = commandBuffer.instance;
		commandBuffer.Record([&](const vk::raii::CommandBuffer &cmd) {
			clearValues[0].color = vk::ClearColorValue{ std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f} }; //color frame
			clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 }; //depth frame

			cmd.beginRenderPass(renderpassInfo, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
			
			cmd.bindVertexBuffers(0, vertexBuffers, offsets);
			cmd.bindIndexBuffer(*indexBuffer.buffer, {}, vk::IndexType::eUint32);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, { *descriptorSet }, nullptr);
			
			cmd.setViewport(0, { viewport });
			cmd.setScissor(0, { scissor });

			cmd.drawIndexed(model.indices.size(), 1, 0, 0, 0);

			cmd.endRenderPass();
		}, {});
		//commandBuffer.instance.end();
		commandBuffer.Submit(graphicQueue, { *imageAvailableSemaphores }, { vk::PipelineStageFlagBits::eColorAttachmentOutput }, { *renderFinishedSemaphores }, fence);

		vk::PresentInfoKHR presentCreateInfo{ 1, &*renderFinishedSemaphores, 1, &*swapChain, &imageIndex, nullptr };
		switch (presentQueue.presentKHR(presentCreateInfo)) {
		case vk::Result::eErrorOutOfDateKHR:
		case vk::Result::eSuboptimalKHR:
			LOG(WARNING) << "OUTDATe";
			break;
		case vk::Result::eSuccess:
			break;
		default:
			LOG(FATAL) << "failed to present swap chain image!";
		}
	}

}

int main(int argc, char *argv[]) {
#pragma region GLog init
	google::InitGoogleLogging(argv[0]);
	//google::SetLogDestination(google::GLOG_INFO, "Logs");
	google::SetStderrLogging(google::GLOG_INFO);
	google::SetLogFilenameExtension(".log");
	defer(google::ShutdownGoogleLogging());
#pragma endregion

	run(argc, argv);
	return 0;
}
