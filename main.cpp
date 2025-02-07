/**
 * @author gh Corgice @IceSandwich
 * @date July 2024
 * @license MIT
 */

#define NOMINMAX

#include <iostream>
#include <vector>
#include <array>
#include <functional>
#include <algorithm>
#include <regex>
#include <map>
#include <tuple>
#include <set>
#include <optional>
#include <iterator>
#include <queue>
#include <assert.h>
#include <fstream>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

struct IWindow {
    virtual bool GetFramebufferSize(int &width, int &height) const = 0;
};

class VulkanSurface {
public:
    VulkanSurface(VkInstance instance, VkSurfaceKHR surface, IWindow *window) : m_instance{instance}, m_surface{surface}, m_window{ window } {

    }
    ~VulkanSurface() {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }

    VkSurfaceKHR Get() const { return m_surface; }
    inline bool GetFramebufferSize(int &width, int &height) const { 
        return m_window->GetFramebufferSize(width, height); 
    }

protected:
    VkInstance m_instance;
    VkSurfaceKHR m_surface;
    IWindow *m_window;
};

class GLFWWindow : public IWindow {
public:
    GLFWWindow(const char *title, const int width = 800, const int height = 600) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

        // glfwSetWindowUserPointer(m_window, this);
        // glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    }

    ~GLFWWindow() {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    virtual bool GetFramebufferSize(int &width, int &height) const {
        glfwGetFramebufferSize(m_window, &width, &height);
        return true;
    }

    bool ShouldClose() {
        return glfwWindowShouldClose(m_window);
    }

    VulkanSurface CreateSurface(VkInstance instance) {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (VkResult result = glfwCreateWindowSurface(instance, m_window, nullptr, &surface); result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
        return VulkanSurface{instance, surface, this};
    }

    std::vector<const char*> GetRequiredExtensions() {
        uint32_t extensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);
        return extensions;
    }

protected:
    GLFWwindow* m_window = nullptr;
};


class VulkanInstance {
public:
    VulkanInstance(const char* appName = "Vulkan", std::vector<const char*> extensions = {}) {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = appName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // check layers
        if constexpr (EnableValidationLayers) {
            uint32_t instanceLayoutCount;
            vkEnumerateInstanceLayerProperties(&instanceLayoutCount, nullptr);
            std::vector<VkLayerProperties> availableInstanceLayers(instanceLayoutCount);
            vkEnumerateInstanceLayerProperties(&instanceLayoutCount, availableInstanceLayers.data());
            
            for (const VkLayerProperties& layer : availableInstanceLayers) {
                std::cout << "[VulkanInstance] Available instance layer: " << layer.layerName << std::endl;
            }

            m_enableValidationLayers = std::all_of(ValidationLayers.begin(), ValidationLayers.end(), [&](const char* layerName){
                return std::any_of(availableInstanceLayers.begin(), availableInstanceLayers.end(), [layerName](VkLayerProperties& layer){
                    return strcmp(layer.layerName, layerName) == 0;
                });
            });

            if (m_enableValidationLayers) {
                std::cout << "[VulkanInstance] Validation layers enabled." << std::endl;
            } else {
                std::cerr << "[VulkanInstance] Validation layers required but can't find matched validation layers. Validation layers disabled." << std::endl;
            }
        } else {
            std::cout << "[VulkanInstance] Validation layers disabled." << std::endl;
        }

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
        debugCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback  = debugCallback;
        debugCreateInfo.pNext = nullptr;

        { // check extensions
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

            for (VkExtensionProperties& extension : availableExtensions) {
                std::cout << "[VulkanInstance] Available extension: " << extension.extensionName << std::endl;
            }

            const std::function<bool(const char*)> checkExtensions = [&availableExtensions](const char* extension)  {
                if (bool found = std::any_of(availableExtensions.begin(), availableExtensions.end(), [extension](VkExtensionProperties& availableExtension) {
                    return strcmp(extension, availableExtension.extensionName) == 0;
                }); !found) {
                    std::cout << "[VulkanInstance][Error] Unsupported extension in arguments: " << extension << std::endl;
                    return false;
                }
                return true;
            };

            if (bool hasExtensions = std::all_of(extensions.begin(), extensions.end(), checkExtensions) & std::all_of(ValidationExtensions.begin(), ValidationExtensions.end(), checkExtensions); !hasExtensions) {
                throw std::runtime_error("Unsupported extension in arguments");
            }
        }

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        if (m_enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
            createInfo.ppEnabledLayerNames = ValidationLayers.data();

            extensions.insert(extensions.end(), ValidationExtensions.begin(), ValidationExtensions.end());
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            createInfo.pNext = &debugCreateInfo;
        }

        if (VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance); result != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }

        if (VkResult result = GetProcAddr<PFN_vkCreateDebugUtilsMessengerEXT>("vkCreateDebugUtilsMessengerEXT")(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger); result!= VK_SUCCESS) {
            throw std::runtime_error("failed to setup debug messenger!");
        }
    }

    ~VulkanInstance() {
        if (m_debugMessenger) {
            GetProcAddr<PFN_vkDestroyDebugUtilsMessengerEXT>("vkDestroyDebugUtilsMessengerEXT")(m_instance, m_debugMessenger, nullptr);
        }
        if (m_instance) {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    template <typename PFN_FUNC, bool THROW_THEN_NOT_FOUND = true>
    PFN_FUNC GetProcAddr(const char* name) {
        PFN_FUNC func = reinterpret_cast<PFN_FUNC>(vkGetInstanceProcAddr(m_instance, name));
        if constexpr (THROW_THEN_NOT_FOUND) {
            if (func == nullptr) {
                throw std::runtime_error("failed to get proc addr!");
            }
        }
        return func;
    }

    void DestroySurfaceKHR(VkSurfaceKHR surface) {
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }

    VkInstance Get() {
        return m_instance;
    }

    bool IsEnableValidationLayers() const {
        return m_enableValidationLayers;
    }

protected:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "[ValidationLayer] " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
protected:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
#ifdef NDEBUG
    static constexpr bool EnableValidationLayers = false;
#else
    static constexpr bool EnableValidationLayers = true;
#endif
    bool m_enableValidationLayers = false;

public:
    constexpr static std::array<const char*, 1> ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    constexpr static std::array<const char*, 1> ValidationExtensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
};

/**
 * Create a Vulkan Device.
 * You can provide a string to describe which device you prefer. Use `;` to separate different device. Use `,` to separate different requirement for each device.
 * Requirement consist of three parts: Queue, Extensions and Device Features. Each part can be:
 * Queue Type: `graphics`, `compute`, `transfer`, `present`
 * Extensions: `swapchain`, `shader non sematic info`
 * Device Features: `sampler anisotropy`, `sampler rateshading`
 * For example, "discrete gpu:graphics,compute,swapchain;cpu" means you want a discrete GPU with graphics, compute and swapchain or a CPU.
 * 
 */
class VulkanDevice {
public:
    enum class QueueType {
        None = 0,
        Graphics,
        Compute,
        Transfer,
        Present,
    };
protected:
    enum class ExtensionType {
        None = 0,
        SwapChainSupported,
        ShaderNonSemanticInfo,
    };
    enum class FeatureType {
        None = 0,
        SamplerAnisotropy,
        SamplerRateShading,
    };
    struct Requirement {
        std::set<QueueType> queueTypes;
        std::set<ExtensionType> extensions;
        std::set<FeatureType> features;
    };
    typedef std::map< VkPhysicalDeviceType, Requirement > PreferMap;
    struct DeviceFindInfo {
        bool result;
        std::string name;
        std::map< QueueType, uint32_t > queueIndices;
        std::set<ExtensionType> supportedExtensions;
        std::set<FeatureType> supportedFeatures;
    };
public:
    VulkanDevice(VulkanInstance &instance, const char* prefer = "discrete gpu", const VulkanSurface *surface = nullptr) : m_instance{ instance }, m_surface{ surface } {
        PreferMap preferMap = parsePrefer(prefer);
        DeviceFindInfo physicalDeviceInfo;
        if (physicalDeviceInfo = findPhysicalDevice(preferMap); !physicalDeviceInfo.result) {
            throw std::runtime_error("failed to find a suitable device");
        }
        if (!createLogicalDevice(physicalDeviceInfo)) {
            throw std::runtime_error("failed to create logical device");
        }
    }
    ~VulkanDevice() {
        if (m_logicalDevice!= nullptr) {
            vkDestroyDevice(m_logicalDevice, nullptr);
        }
    }

    struct Queue {
        VkQueue raw;
        uint32_t index;
        Queue(VkQueue _raw, uint32_t _index) : raw{ _raw }, index{ _index } { }
    };
    Queue GetGraphicsQueue() {
        return Queue{ m_graphicQueue, m_queueIndices[QueueType::Graphics] };
    }
    Queue GetComputeQueue() {
        return Queue{ m_computeQueue, m_queueIndices[QueueType::Compute] };
    }
    // VkQueue GetGraphicQueue() { return m_graphicQueue; }
    // VkQueue GetComputeQueue() { return m_computeQueue; }
    // VkQueue GetTransferQueue() { return m_transferQueue; }
    // VkQueue GetPresentQueue() { return m_presentQueue; }
    VkDevice Get() const { return m_logicalDevice; }
    VkPhysicalDevice GetPhysicalDevice() { return m_physicalDevice; }
    const VulkanSurface* GetSurface() const { return m_surface; }
    uint32_t GetQueueIndex(QueueType type) const {
        return m_queueIndices.at(type);
    }
    
protected:
    static PreferMap parsePrefer(std::string prefer) {
        static const std::map<std::string, VkPhysicalDeviceType> DeviceTypeStr2Type{
            { "discrete gpu", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU },
            { "integrated gpu", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU },
            { "virtual gpu", VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU }
        };
        static const std::map<std::string, QueueType> QueueStr2Type{
            { "graphics", QueueType::Graphics  },
            { "compute", QueueType::Compute  },
            { "transfer", QueueType::Transfer   },
            { "present", QueueType::Present    },
        };
        static const std::map<std::string, ExtensionType> ExtensionStr2Type{
            { "swapchain", ExtensionType::SwapChainSupported },
            { "shader non sematic info", ExtensionType::ShaderNonSemanticInfo },
         };
        static const std::map<std::string, FeatureType> FeatureStr2Type{
            { "anisotropy", FeatureType::SamplerAnisotropy },
            { "rate shading", FeatureType::SamplerRateShading },
        };
        
        std::regex reSplitDevices{"([[:alpha:] ]+)(:([[:alpha:] ,]+))?", std::regex::icase };
        std::regex reSplitRequirements{"([[:alpha:] ]+)", std::regex::icase };

        PreferMap preferMap;

        // search for device type
        for (std::smatch deviceMatches; std::regex_search(prefer, deviceMatches, reSplitDevices); prefer = deviceMatches.suffix()) {
            std::string deviceType = deviceMatches[1].str();
            if (DeviceTypeStr2Type.count(deviceType) == 0) {
                throw std::runtime_error("invalid device type: " + deviceType);
            }
            VkPhysicalDeviceType deviceTypeValue = DeviceTypeStr2Type.at(deviceType);
            // newer one will overwrite the previous one
            preferMap[deviceTypeValue] = {};
        
            // check if there are requirements for this device
            if (!deviceMatches[3].matched) continue;

            // search for requirements and split them into `Queue`, `Extension`, and `Feature`
            std::string requirementsStr = deviceMatches[3].str();
            for (std::smatch requirementMatches; std::regex_search(requirementsStr, requirementMatches, reSplitRequirements); requirementsStr = requirementMatches.suffix()) {
                std::string requirementStr = requirementMatches.str();
                std::transform(requirementStr.begin(), requirementStr.end(), requirementStr.begin(), [](char c) { return std::tolower(c); });
                if (auto found = QueueStr2Type.find(requirementStr); found != QueueStr2Type.end()) {
                    preferMap[deviceTypeValue].queueTypes.insert(found->second);
                } else if (auto found = ExtensionStr2Type.find(requirementStr); found!= ExtensionStr2Type.end()) {
                    preferMap[deviceTypeValue].extensions.insert(found->second);
                } else if (auto found = FeatureStr2Type.find(requirementStr); found!= FeatureStr2Type.end()) {
                    preferMap[deviceTypeValue].features.insert(found->second); 
                } else {
                    throw std::runtime_error("invalid requirement: " + requirementStr);
                }
            }
        }
        return preferMap;
    }

    static const std::map< ExtensionType, const char* > ExtensionType2VkName;
    DeviceFindInfo findPhysicalDevice(const PreferMap& prefer) {
        DeviceFindInfo ret;
        ret.result = false;

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance.Get(), &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance.Get(), &deviceCount, devices.data());

        for (const VkPhysicalDevice& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            std::cout << "[VulkanDevice] Found device: " << deviceProperties.deviceName << std::endl;

            // skip devices that are not meet required device type
            if (prefer.find(deviceProperties.deviceType) == prefer.end())  continue;
            const Requirement& requirements = prefer.at(deviceProperties.deviceType);

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            bool matchQueues = std::all_of(requirements.queueTypes.begin(), requirements.queueTypes.end(), [&](QueueType queueType) {
                switch (queueType) {
                case QueueType::Graphics:
                    if (auto found = std::find_if(queueFamilies.begin(), queueFamilies.end(), [](VkQueueFamilyProperties& qfp) {
                        return (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT)!= 0;
                    }); found!= queueFamilies.end()) {
                        ret.queueIndices[QueueType::Graphics] = std::distance(queueFamilies.begin(), found);
                        return true;
                    }
                    break;
                case QueueType::Compute:
                    if (auto found = std::find_if(queueFamilies.begin(), queueFamilies.end(), [](VkQueueFamilyProperties& qfp) {
                        return ((qfp.queueFlags & VK_QUEUE_COMPUTE_BIT)!= 0);
                     }); found!= queueFamilies.end())  {
                        ret.queueIndices[QueueType::Compute] = std::distance(queueFamilies.begin(), found);
                        return true;
                    }
                    break;
                case QueueType::Transfer:
                    if (auto found = std::find_if(queueFamilies.begin(), queueFamilies.end(), [](VkQueueFamilyProperties& qfp) {
                        return ((qfp.queueFlags & VK_QUEUE_TRANSFER_BIT)!= 0);
                    }); found!= queueFamilies.end())   {
                        ret.queueIndices[QueueType::Transfer] = std::distance(queueFamilies.begin(), found);
                        return true;
                    }
                    break;
                case QueueType::Present:
                    if (m_surface == nullptr) {
                        throw std::runtime_error("Present queue requested but no surface provided");
                    }

                    for (int i = 0; i < queueFamilies.size(); ++i) {
                        VkBool32 presentSupport = false;
                        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface->Get(), &presentSupport);
                        if (presentSupport) {
                            ret.queueIndices[QueueType::Present] = i;
                            return true;
                        }
                    }
                    break;
                default:
                    throw std::logic_error("should not reach here");
                }
                return false;
            });

            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            for (const VkExtensionProperties& availableExt : availableExtensions) {
                std::cout << "[VulkanDevice][" << deviceProperties.deviceName << "] Available extension: " << availableExt.extensionName << std::endl;
            }

            bool matchExtensions = std::all_of(requirements.extensions.begin(), requirements.extensions.end(), [&](const VulkanDevice::ExtensionType ext) {
                return std::any_of(availableExtensions.begin(), availableExtensions.end(), [&](const VkExtensionProperties& availableExt) {
                    if (strcmp(availableExt.extensionName, ExtensionType2VkName.at(ExtensionType::SwapChainSupported)) == 0) {
                        if (m_surface == nullptr) {
                            throw std::runtime_error("Swap chain requested but no surface provided");
                        }

                        VkSurfaceKHR surface = m_surface->Get();

                        VkSurfaceCapabilitiesKHR capabilities;
                        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

                        uint32_t formatCount;
                        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
                        if (formatCount == 0) return false;

                        uint32_t presentModeCount;
                        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
                        if (presentModeCount == 0) return false;

                        return true;
                    }
                    return strcmp(availableExt.extensionName, ExtensionType2VkName.at(ext)) == 0;
                });
            });

            VkPhysicalDeviceFeatures availableFeatures;
            vkGetPhysicalDeviceFeatures(device, &availableFeatures);

            bool matchFeatures = std::all_of(requirements.features.begin(), requirements.features.end(), [&](const VulkanDevice::FeatureType requireFeature) {
                switch (requireFeature) {
                    case FeatureType::SamplerAnisotropy:
                        return availableFeatures.samplerAnisotropy == VK_TRUE;
                    case FeatureType::SamplerRateShading:
                        return availableFeatures.sampleRateShading == VK_TRUE;
                }
                return false;
            });

            if (matchQueues && matchExtensions && matchFeatures) {
                m_physicalDevice = device;
                ret.result = true;
                ret.name = deviceProperties.deviceName;
                ret.supportedExtensions = requirements.extensions;
                ret.supportedFeatures = requirements.features;
                break;
            }
        }

        return ret;
    }
    bool createLogicalDevice(const DeviceFindInfo& deviceInfo) {
        std::set<uint32_t> uniqueQueueFamilies;
        std::transform(deviceInfo.queueIndices.begin(), deviceInfo.queueIndices.end(), std::inserter(uniqueQueueFamilies, uniqueQueueFamilies.begin()), [](const std::pair<QueueType, uint32_t>& pair) {
            return pair.second;
        });

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(uniqueQueueFamilies.size());
        float queuePriorities = 1.0f;
        std::transform(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end(), queueCreateInfos.begin(), [&queuePriorities](uint32_t index) {
            VkDeviceQueueCreateInfo queueCreateInfo;
            queueCreateInfo.sType            = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = index;
            queueCreateInfo.queueCount       = 1;
            queueCreateInfo.pQueuePriorities  = &queuePriorities;
            queueCreateInfo.pNext           = nullptr;
            queueCreateInfo.flags           = 0;
            return queueCreateInfo;
        });

        std::vector<const char*> deviceExtensions;
        std::string deviceExtensionsString = "";
        std::transform(deviceInfo.supportedExtensions.begin(), deviceInfo.supportedExtensions.end(), std::back_inserter(deviceExtensions), [&](const VulkanDevice::ExtensionType extensionType) {
            const char* vkName = ExtensionType2VkName.at(extensionType);
            deviceExtensionsString += vkName;
            deviceExtensionsString += ", ";
            return vkName;
        });

        VkPhysicalDeviceFeatures deviceFeatures = {};
        std::string deviceFeaturesString = "";
        for (FeatureType feature : deviceInfo.supportedFeatures) {
            switch (feature) {
                case FeatureType::SamplerAnisotropy:
                    deviceFeatures.samplerAnisotropy = VK_TRUE;
                    deviceFeaturesString += "anisotropy, ";
                    break;
                case FeatureType::SamplerRateShading:
                    deviceFeatures.sampleRateShading = VK_TRUE;
                    deviceFeaturesString += "rate shading, ";
                    break;
            }
        }

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType            = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.enabledExtensionCount = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames   = deviceExtensions.data();
        createInfo.pEnabledFeatures   = &deviceFeatures;
        if (m_instance.IsEnableValidationLayers()) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_instance.ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_instance.ValidationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
        }

        if (VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice); result != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }
        std::cout << "[VulkanDevice] Using device: " << deviceInfo.name << std::endl;
        std::cout << "[VulkanDevice] Device features: " << deviceFeaturesString << std::endl;
        std::cout << "[VulkanDevice] Device extensions: " << deviceExtensionsString << std::endl;

        for (const auto& [type, index] : deviceInfo.queueIndices) {
            switch (type) {
                case QueueType::Graphics:
                    vkGetDeviceQueue(m_logicalDevice, index, 0, &m_graphicQueue);
                    std::cout << "[VulkanDevice] Using graphic queue: " << index << std::endl;
                    break;
                case QueueType::Compute:
                    vkGetDeviceQueue(m_logicalDevice, index, 0, &m_computeQueue);
                    std::cout << "[VulkanDevice] Using compute queue: " << index << std::endl;
                    break;
                case QueueType::Transfer:
                    vkGetDeviceQueue(m_logicalDevice, index, 0, &m_transferQueue);
                    std::cout << "[VulkanDevice] Using transfer queue: " << index << std::endl;
                    break;
                case QueueType::Present:
                    vkGetDeviceQueue(m_logicalDevice, index, 0, &m_presentQueue);
                    std::cout << "[VulkanDevice] Using present queue: " << index << std::endl;
                    break;
            }
        }
        m_queueIndices = deviceInfo.queueIndices;
        return true;
    }
protected:
    VulkanInstance &m_instance;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    const VulkanSurface* m_surface = nullptr;

    VkQueue m_graphicQueue = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;
    VkQueue m_transferQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    std::map< VulkanDevice::QueueType, uint32_t > m_queueIndices;
};
const std::map< VulkanDevice::ExtensionType, const char* > VulkanDevice::ExtensionType2VkName = {
    { ExtensionType::SwapChainSupported, VK_KHR_SWAPCHAIN_EXTENSION_NAME },
    { ExtensionType::ShaderNonSemanticInfo, VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME },
};

class VulkanCommand {
private:
    struct CommandPool {
        VkCommandPool pool;
        std::vector<VkCommandBuffer> buffers;
    };
public:
    class Builder {
    public:
        typedef size_t CommandBufferId;
        CommandBufferId AddCommandBuffer(VulkanDevice::Queue queue, uint32_t count = 1) {
            m_commandbuffers.push_back(std::make_pair(queue.index, count));
            return m_commandbuffers.size() - 1;
        }
        std::unique_ptr<VulkanCommand> Build(VulkanDevice& device) {
            std::unique_ptr<VulkanCommand> vc{ new VulkanCommand{ device } };
            
            std::set<uint32_t> uniqueQueues;
            std::transform(m_commandbuffers.begin(), m_commandbuffers.end(), std::inserter(uniqueQueues, uniqueQueues.begin()), [](const auto& pair) { return pair.first; });
            vc->m_datas.resize(uniqueQueues.size());

            std::map<uint32_t, uint32_t> mapQueueToIndex;
            int i = 0;
            for (uint32_t queueIndex : uniqueQueues) {
                VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0 };
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = queueIndex;

                if (vkCreateCommandPool(device.Get(), &poolInfo, nullptr, &vc->m_datas[i].pool) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create command pool!");
                }

                mapQueueToIndex[queueIndex] = i;

                ++i;
            }

            for (auto& [queueIdx, count] : m_commandbuffers) {
                CommandPool cp = vc->m_datas[mapQueueToIndex[queueIdx]];
                cp.buffers.resize(count);

                VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, 0 };
                allocInfo.commandBufferCount = count;
                allocInfo.commandPool = cp.pool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                
                if (vkAllocateCommandBuffers(device.Get(), &allocInfo, cp.buffers.data()) != VK_SUCCESS) {
                    throw std::runtime_error("failed to allocate command buffers!");
                }
            }

            return vc;
        }
    protected:
        // first one is queue index, second one is count
        std::vector< std::pair<uint32_t, uint32_t> > m_commandbuffers;
    };
    ~VulkanCommand() {
        for (CommandPool &cp : m_datas) {
            if (cp.pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(m_device.Get(), cp.pool, nullptr);
            }
        }
    }
    VulkanCommand(VulkanCommand&& other) : m_device{ other.m_device } {
        std::swap(m_datas, other.m_datas);
    }
protected:
    VulkanCommand(VulkanDevice& device) : m_device{ device } { };
    VulkanDevice& m_device;
    std::vector<CommandPool> m_datas;
};

class VulkanMemory {
public:
    enum class StoreLocation {
        Local,
        Device
    };
    VulkanMemory(VulkanDevice &device, VkMemoryRequirements memRequirements, StoreLocation storeLocation) : m_device{ device } {
        VkMemoryPropertyFlagBits memPropFlags;
        switch (m_storeLocation) {
            case StoreLocation::Local : 
                memPropFlags = static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                break;
            case StoreLocation::Device : 
                memPropFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; 
                break;
        }

        VkMemoryAllocateInfo memAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr};
        memAllocInfo.allocationSize = memRequirements.size;
        memAllocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memPropFlags);
    }
    ~VulkanMemory() {
        vkFreeMemory(m_device.Get(), m_memory, nullptr);
    }

    inline VkDeviceMemory GetMemory() const noexcept{
        return m_memory;
    }
    inline StoreLocation GetStoreLocation() const noexcept{
        return m_storeLocation;
    }
protected:
    VulkanDevice& m_device;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    StoreLocation m_storeLocation = StoreLocation::Local;
protected:
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_device.GetPhysicalDevice(), &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i){
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }
};

class IVulkanImage {
public:
    enum class ImageType {
        Color,
        DepthStencil,
        Present
    };
    VkImageView GetImageView() const noexcept{
        return m_imageView;
    }
    virtual ~IVulkanImage() = default;

    inline ImageType GetImageType() const noexcept{
        return m_type;
     }
     inline VkFormat GetFormat() const noexcept{
        return m_format;
     }
protected:
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;

    VulkanDevice &m_device;
    uint32_t m_width, m_height;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    std::unique_ptr<VulkanMemory> m_memory = nullptr;
    VulkanMemory::StoreLocation m_storeLocation = VulkanMemory::StoreLocation::Local;
    ImageType m_type;

protected:
    IVulkanImage(VulkanDevice& device, uint32_t width, uint32_t height, VkFormat format, VulkanMemory::StoreLocation storeLocation, ImageType type) : m_device{device}, m_width{width}, m_height{height}, m_format{format}, m_storeLocation{storeLocation}, m_type{ type } {

    }
};

class VulkanColorImage : public IVulkanImage {
public:
    VulkanColorImage(VulkanDevice& device, uint32_t width, uint32_t height, VkFormat format, VulkanMemory::StoreLocation storeLocation) : IVulkanImage{device, width, height, format, storeLocation, IVulkanImage::ImageType::Color } {
        create();
    }
    virtual ~VulkanColorImage() override {
        cleanup();
    }
    void Recreate() {
        cleanup();
        create();
    }
protected:
    void cleanup() {
        if (m_imageView!= VK_NULL_HANDLE) {
			vkDestroyImageView(m_device.Get(), m_imageView, nullptr);
		}
		if (m_image!= VK_NULL_HANDLE) {
			vkDestroyImage(m_device.Get(), m_image, nullptr);
		}
		if (m_memory != nullptr) {
			m_memory.reset();
		}
    }
    void create() {
        // create image
        VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0 };
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = m_format;
		imageInfo.extent.width = m_width;
		imageInfo.extent.height = m_height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.queueFamilyIndexCount = 0;
		imageInfo.pQueueFamilyIndices = nullptr;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (vkCreateImage(m_device.Get(), &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

        // create image view
        VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0 };
		imageViewInfo.image = m_image;
		imageViewInfo.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = m_format;
        imageViewInfo.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel  = 0;
		imageViewInfo.subresourceRange.levelCount  = 1;
		imageViewInfo.subresourceRange.baseArrayLayer  = 0;
		imageViewInfo.subresourceRange.layerCount  = 1;

        if (vkCreateImageView(m_device.Get(), &imageViewInfo, nullptr, &m_imageView)!= VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}

        // get image memory requirements
        VkMemoryRequirements imageMemoryRequirements;
        vkGetImageMemoryRequirements(m_device.Get(), m_image, &imageMemoryRequirements);

        m_memory = std::make_unique<VulkanMemory>(m_device, imageMemoryRequirements, m_storeLocation);
    }
};

class VulkanPresentImage : public VulkanColorImage {
    VulkanPresentImage(VulkanDevice& device, uint32_t width, uint32_t height, VkFormat format, VulkanMemory::StoreLocation storeLocation) : 
        VulkanColorImage{ device, width, height, format, storeLocation }
    {
        m_type = IVulkanImage::ImageType::Present;
    }
};

class VulkanFramebufferResource {
public:
    struct Attachments {
        std::vector<VkAttachmentReference> color;
        VkAttachmentReference depth;
        std::vector<VkAttachmentReference> resolve;
        std::vector<VkAttachmentReference> input;

        std::vector<VkAttachmentDescription> descriptions;
    };
    VulkanFramebufferResource(VulkanDevice& device) : m_device{device} {

    }
    ~VulkanFramebufferResource() {
        for (auto resource : m_resources) {
            delete resource;
        }
    }
    void AddResource(std::unique_ptr<IVulkanImage> imageResource) {
        m_resources.push_back(imageResource.release());
    }
    Attachments MakeAttachments() {
        Attachments attachments;
        for (auto resource : m_resources) {
            switch (resource->GetImageType()) {
                case IVulkanImage::ImageType::Present:
                case IVulkanImage::ImageType::Color: {
                    VkAttachmentDescription& attachment = attachments.descriptions.emplace_back();
                    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                    attachment.format = resource->GetFormat();
                    attachment.flags = 0;

                    VkAttachmentReference& attachmentRef = attachments.color.emplace_back();
                    attachmentRef.attachment = static_cast<uint32_t>(attachments.descriptions.size()) - 1;
                    attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    break;
                }
                case IVulkanImage::ImageType::DepthStencil: {
                    VkAttachmentDescription& attachment  = attachments.descriptions.emplace_back();
                    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    attachment.format = findDepthFormat(VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
                    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    attachment.flags = 0;

                    VkAttachmentReference& attachmentRef = attachments.depth;
                    attachmentRef.attachment  = static_cast<uint32_t>(attachments.descriptions.size()) - 1;
                    attachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    break;
                }
            }
        }
        return attachments;
    }
protected:
    VulkanDevice& m_device;
    std::vector<IVulkanImage*> m_resources; // owned, remember to delete!

protected:
    VkFormat findDepthFormat(VkImageTiling tiling, VkFormatFeatureFlags features) {
        std::array<VkFormat, 3> candidates = { 
            VK_FORMAT_D32_SFLOAT, 
            VK_FORMAT_D32_SFLOAT_S8_UINT, 
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (auto format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_device.GetPhysicalDevice(), format, &props);
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("failed to find suitable depth format!");
    }
};

class VulkanFramebuffer {
public:
    VulkanFramebuffer(VulkanDevice& device, uint32_t width, uint32_t height, const std::vector<VkImageView>& attachments, VkRenderPass renderPass) : 
        m_device{device}, m_renderPass{renderPass} {
        VkFramebufferCreateInfo framebufferInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0 };
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments    = attachments.data();
        framebufferInfo.width           = width;
        framebufferInfo.height          = height;
        framebufferInfo.layers          = 1;
        if (vkCreateFramebuffer(m_device.Get(), &framebufferInfo, nullptr, &m_framebuffer)!= VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
    ~VulkanFramebuffer() {
        vkDestroyFramebuffer(m_device.Get(), m_framebuffer, nullptr);
    }
protected:
    VulkanDevice &m_device;
    VkRenderPass m_renderPass; // borrow. do not delete!

    VkFramebuffer m_framebuffer;
};

class VulkanSwapChain {
public:
    VulkanSwapChain(VulkanDevice& device) : m_device{device} {
        if (device.GetSurface() == VK_NULL_HANDLE) {
            throw std::runtime_error("Swapchain require a surface provided for device.");
        }
        m_surface = device.GetSurface()->Get();

        createSwapChain();
        std::cout << "[VulkanSwapChain] Created " << m_images.size() << " swapchain images." << std::endl;
    }
    ~VulkanSwapChain() {
        cleanupSwapChain();
    }
    void RecreateSwapChain() {
        cleanupSwapChain();
        createSwapChain();
    }
    inline uint32_t GetWidth() const {
        return m_extent.width;
    }
    inline uint32_t GetHeight() const {
        return m_extent.height;
    }
    inline VkFormat GetFormat() const {
        return m_format;
    }
    inline VkSampleCountFlagBits GetSampleCount() const {
        return VK_SAMPLE_COUNT_1_BIT;
    }
    VulkanDevice& GetDevice() const { return m_device; }
    inline uint32_t Count() const {
        return m_imageCount;
    }
    

    template <typename ...Args>
    std::vector<VulkanFramebuffer>& RecreateFramebuffers(VkRenderPass renderPass) {
        const size_t swapChainImageCount = m_images.size();

        cleanupResources();
        ( m_framebufferResources.emplace_back(new Args{ m_device, m_width, m_height, m_format, VulkanMemory::StoreLocation::Device }), ... );

        m_framebuffers.clear();
        for (int i = 0; i < swapChainImageCount; ++i) {
            std::vector<VkImageView> attachments{ m_imageViews[i] };
            std::transform(m_framebufferResources.begin(), m_framebufferResources.end(), std::back_inserter(attachments), [](auto& resource) {
                return resource->GetImageView();
            });

            m_framebuffers.emplace_back(m_device, m_width, m_height, attachments, renderPass);
        }
        return m_framebuffers;
    }
protected:
    VkSurfaceFormatKHR chooseSwapSurfaceFormat() {
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.GetPhysicalDevice(), m_surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> availableFormats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.GetPhysicalDevice(), m_surface, &formatCount, availableFormats.data());

        auto found = std::find_if(availableFormats.cbegin(), availableFormats.cend(), [](const VkSurfaceFormatKHR& availableFormat) -> bool {
			return availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		});
        if (found == availableFormats.cend()) {
            return availableFormats[0];
        }
		
        return *found;
    }
    VkPresentModeKHR chooseSwapPresentMode() {
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.GetPhysicalDevice(), m_surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.GetPhysicalDevice(), m_surface, &presentModeCount, availablePresentModes.data());

        auto found = std::find(availablePresentModes.cbegin(), availablePresentModes.cend(), VK_PRESENT_MODE_MAILBOX_KHR);
        if (found == availablePresentModes.cend()) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }
        
        return *found;
    }
    VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}

        int width, height;
        m_device.GetSurface()->GetFramebufferSize(width, height);
        
        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }

    void cleanupSwapChain() {
        cleanupResources();
        m_framebuffers.clear();
        for (auto &imageView : m_imageViews) {
            vkDestroyImageView(m_device.Get(), imageView, nullptr);
        }
        vkDestroySwapchainKHR(m_device.Get(), m_swapChain, nullptr);
    }
    void createSwapChain() {
        VkSurfaceCapabilitiesKHR capabilities = {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.GetPhysicalDevice(), m_surface, &capabilities);

        VkSurfaceFormatKHR format = chooseSwapSurfaceFormat();
        VkPresentModeKHR presentMode = chooseSwapPresentMode();
        m_extent = chooseSwapExtent(capabilities);

        m_imageCount = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

		VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0 };
		createInfo.clipped = VK_TRUE;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.imageArrayLayers = 1;
		createInfo.imageColorSpace = format.colorSpace;
		createInfo.imageExtent = m_extent;
		createInfo.imageFormat = format.format;

        std::vector<uint32_t> queueFamilyIndices;
        queueFamilyIndices.push_back(m_device.GetQueueIndex(VulkanDevice::QueueType::Graphics));
        queueFamilyIndices.push_back(m_device.GetQueueIndex(VulkanDevice::QueueType::Present));
        if (queueFamilyIndices[0]!= queueFamilyIndices[1]) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
			createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.minImageCount = m_imageCount;
		createInfo.oldSwapchain = VK_NULL_HANDLE;
		createInfo.presentMode = presentMode;
		createInfo.preTransform = capabilities.currentTransform;
		createInfo.surface = m_surface;

        const VkDevice device = m_device.Get();
		if (VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapChain); result != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}
        m_format = format.format;

        // get swap chain images
		vkGetSwapchainImagesKHR(device, m_swapChain, &m_imageCount, nullptr);
		m_images.resize(m_imageCount);
		vkGetSwapchainImagesKHR(device, m_swapChain, &m_imageCount, m_images.data());

        // create image views
        m_imageViews.resize(m_imageCount);
		for (size_t i = 0; i < m_images.size(); ++i) {
			VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0 };
			createInfo.image = m_images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = format.format;
			createInfo.components.r  = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g  = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b  = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a  = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

            if (VkResult result = vkCreateImageView(device, &createInfo, nullptr, &m_imageViews[i]); result!= VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
        }
    }
    void cleanupResources() {
        for (IVulkanImage* image : m_framebufferResources) {
            delete image;
        }
        m_framebufferResources.clear();
    }
protected:
    VulkanDevice& m_device;
    VkSurfaceKHR m_surface; // borrow, do not destroy!

    VkSwapchainKHR m_swapChain;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkExtent2D m_extent;
    VkFormat m_format;
    uint32_t m_imageCount;

    std::vector<VulkanFramebuffer> m_framebuffers;
    std::vector<IVulkanImage*> m_framebufferResources; // own, remember to destroy!
};

struct MinMax {
    float min, max;
    MinMax() : min(0.f), max(1.f) {}
    MinMax(float _min, float _max) : min(_min), max(_max) {}
    void Range(float _min, float _max) {
        min = _min;
        max = _max;
    }
};

#if 0
class VulkanPipeline {
public:
    class ShaderModule;
private:
    static VkPipelineShaderStageCreateInfo generateShaderStageCreateInfo(const ShaderModule& sm);
public:
    class ShaderModule {
    public:
        enum class Type {
            Vertex,
            Tessellation,
            Geometry,
            Fragment,
            Compute
        };
        ShaderModule(Type type) : m_type(type) { }
        ~ShaderModule()  {
            Reset();
        }
        inline void Reset() {
            m_loaded = false;
        }
        inline bool Empty() const {
            return !m_loaded;
        }

        bool LoadFromFile(const std::string& filename, std::string entryName = "main") {
            m_binaryCode = readFile(filename);
            // m_module = createShaderModule(code);
            m_entryName = entryName;
            m_loaded = true;
            return m_loaded;
        }

    protected:
        Type m_type;
        std::vector<char> m_binaryCode;
        std::string m_entryName;
        bool m_loaded = false;
    private:
    	static std::vector<char> readFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }

            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();
            return buffer;
        }
        VkShaderModule createShaderModule(const std::vector<char>& code) {
            VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0 };
            createInfo.codeSize = code.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(m_device.Get(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                throw std::runtime_error("failed to create shader module!");
            }

            return shaderModule;
        }
    
        static friend VkPipelineShaderStageCreateInfo VulkanPipeline::generateShaderStageCreateInfo(const ShaderModule& sm);
    };
    
public:
    class VertexInput;
private: // exported interfaces from `VertexInput` to `VulkanPipeline`
    static VkPipelineVertexInputStateCreateInfo generateInputStateCreateInfo(const VulkanPipeline::VertexInput &vi, std::vector<VkVertexInputBindingDescription> &cachedBindingsDesc, std::vector<VkVertexInputAttributeDescription> &cachedAttributeDesc);
    static VkPipelineInputAssemblyStateCreateInfo generateInputAssemblyStateCreateInfo(const VulkanPipeline::VertexInput &vi);
public:
    class VertexInput {
    public:
        typedef uint32_t BindingId;
    protected:
        enum class AttributeFormat {
            Float = VK_FORMAT_R32_SFLOAT,
            Vec2 = VK_FORMAT_R32G32_SFLOAT,
            Vec3 = VK_FORMAT_R32G32B32_SFLOAT,
            Vec4 = VK_FORMAT_R32G32B32A32_SFLOAT,
            IVec2 = VK_FORMAT_R32G32_SINT,
            UVec4 = VK_FORMAT_R32G32B32A32_UINT,
            Double = VK_FORMAT_R64_SFLOAT
        };
        class AttributeBase {
            friend VertexInput;
        protected:
            constexpr AttributeBase(uint32_t location, AttributeFormat format, uint32_t offset) : m_location(location), m_format(format), m_offset(offset) { }
            AttributeFormat m_format;
            uint32_t m_location;
            uint32_t m_offset;
        };

        struct BindingDescription {
            BindingId id;
            VkVertexInputBindingDescription vkDescription;
            std::vector<VkVertexInputAttributeDescription> attributes;
        };
    public:
        struct Vec2Attribute : public AttributeBase {
            constexpr Vec2Attribute(uint32_t location, uint32_t offset) : AttributeBase(location, AttributeFormat::Vec2, offset) { }
        };
        struct Vec3Attribute  : public AttributeBase  {
            constexpr Vec3Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::Vec3, offset)  {  }
        };
        struct Vec4Attribute  : public AttributeBase  {
            constexpr Vec4Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::Vec4, offset)  {  }
        };
        struct IVec2Attribute  : public AttributeBase  {
            constexpr IVec2Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::IVec2, offset)  {  }
        };
        struct UVec4Attribute  : public AttributeBase  {
            constexpr UVec4Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::UVec4, offset)  {  }
        };
        struct DoubleAttribute  : public AttributeBase  {
            constexpr DoubleAttribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::Double, offset)  {  }
        };

        BindingId AddVertexAttributes(std::vector<AttributeBase> attributes, uint32_t strideOfAllAttributes) {
            BindingDescription& desc = m_bindings.emplace_back();
            desc.id = m_bindings.size() - 1;

            desc.vkDescription.binding = 0;
            desc.vkDescription.stride = strideOfAllAttributes;
            desc.vkDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            for (const AttributeBase& attribute : attributes) {
                VkVertexInputAttributeDescription vkAttribute = {};
                vkAttribute.binding = 0;
                vkAttribute.location = attribute.m_location;
                vkAttribute.format = static_cast<VkFormat>(attribute.m_format);
                vkAttribute.offset = attribute.m_offset;

                desc.attributes.push_back(vkAttribute);
            }

            return desc.id;
        }
        
    protected:
        std::vector<BindingDescription> m_bindings;
        VkPrimitiveTopology m_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    private:
        // export interfaces
        static friend VkPipelineVertexInputStateCreateInfo VulkanPipeline::generateInputStateCreateInfo(const VulkanPipeline::VertexInput &vi, std::vector<VkVertexInputBindingDescription> &cachedBindingsDesc, std::vector<VkVertexInputAttributeDescription> &cachedAttributeDesc);
        static friend VkPipelineInputAssemblyStateCreateInfo VulkanPipeline::generateInputAssemblyStateCreateInfo(const VulkanPipeline::VertexInput &vi);
    };

public:    
    struct VulkanDepth {
        MinMax bounds;
    };
private:
    static VkPipelineDepthStencilStateCreateInfo generateDepthStencilStateCreateInfo(const VulkanDepth &depth);

public:
    struct GraphicsConfig {
        ShaderModule vertexShader;
        ShaderModule tessellationShader;
        ShaderModule geometryShader;
        ShaderModule fragmentShader;
        VertexInput vertexInput;
        VulkanDepth depthStencil;

        GraphicsConfig(VulkanDevice& device) :
            vertexShader{  ShaderModule::Type::Vertex },
            tessellationShader{  ShaderModule::Type::Tessellation },
            geometryShader{  ShaderModule::Type::Geometry },
            fragmentShader{  ShaderModule::Type::Fragment }
        {

        }
    };

    static VulkanPipeline Create(GraphicsConfig& config) {
        VulkanPipeline pp;
        
        return pp;
    }

protected:
    VulkanPipeline() = default;
};

// remember to check ShaderModule::Empty() to ensure it's not empty;
VkPipelineShaderStageCreateInfo VulkanPipeline::generateShaderStageCreateInfo(const ShaderModule& sm) {
    VkPipelineShaderStageCreateInfo shaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0 };
	shaderStageInfo.module = sm.m_module;
	shaderStageInfo.pName = sm.m_entryName.c_str();
	shaderStageInfo.pSpecializationInfo = nullptr;
    switch (sm.m_type) {
        case ShaderModule::Type::Vertex:
            shaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ShaderModule::Type::Tessellation:
            throw std::runtime_error("tessellation not supported");
        case ShaderModule::Type::Geometry:
            shaderStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case ShaderModule::Type::Fragment:
            shaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
    }

    return shaderStageInfo;
}

VkPipelineVertexInputStateCreateInfo VulkanPipeline::generateInputStateCreateInfo(const VulkanPipeline::VertexInput &vi, std::vector<VkVertexInputBindingDescription> &cachedBindingsDesc, std::vector<VkVertexInputAttributeDescription> &cachedAttributeDesc) {
    // clear previous data
    cachedBindingsDesc.clear();
    cachedAttributeDesc.clear();

    { // check all attributes have different locations
        std::set<uint32_t> uniqueLocations;
        for (const VulkanPipeline::VertexInput::BindingDescription& binding : vi.m_bindings) {
            for (const VkVertexInputAttributeDescription& attribute : binding.attributes) {
                if (uniqueLocations.find(attribute.location)!= uniqueLocations.end()) {
                    throw std::runtime_error("Attributes location " + std::to_string(attribute.location) + " used more than twice.");
                }
                uniqueLocations.insert(attribute.location);
            }
        }
    }

    // parse bindings & attributes
    for (const VulkanPipeline::VertexInput::BindingDescription& binding : vi.m_bindings) {
        cachedBindingsDesc.push_back(binding.vkDescription);

        for (const VkVertexInputAttributeDescription& attribute : binding.attributes) {
            cachedAttributeDesc.push_back(attribute);
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0 };
    vertexInputInfo.vertexAttributeDescriptionCount = cachedAttributeDesc.size();
    vertexInputInfo.pVertexAttributeDescriptions = cachedAttributeDesc.data();
    vertexInputInfo.vertexBindingDescriptionCount = cachedBindingsDesc.size();
    vertexInputInfo.pVertexBindingDescriptions = cachedBindingsDesc.data();

    return vertexInputInfo;
}

VkPipelineInputAssemblyStateCreateInfo VulkanPipeline::generateInputAssemblyStateCreateInfo(const VulkanPipeline::VertexInput &vi) {
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0 };
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    inputAssembly.topology = vi.m_topology;

    return inputAssembly;
}

VkPipelineDepthStencilStateCreateInfo VulkanPipeline::generateDepthStencilStateCreateInfo(const VulkanDepth &depth) {
    VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0 };
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = depth.bounds.min;
	depthStencil.maxDepthBounds = depth.bounds.max;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};

    return depthStencil;
}
#endif

class DescriptorSet {
public:
    enum class Type {
        Uniform,
        ImageSampler,
        StorageBuffer
    };
protected:
    struct DescriptorBase {
        VkDescriptorSetLayoutBinding vkBinding;
        DescriptorBase(uint32_t bindPoint, VkDescriptorType type, uint32_t count, VkShaderStageFlags access) {
            vkBinding.binding = bindPoint;
            vkBinding.descriptorType = type;
            vkBinding.descriptorCount = count;
            vkBinding.stageFlags = access;
            vkBinding.pImmutableSamplers = nullptr;
        }
    };
    struct DescriptorSetDescription {
        VkDescriptorSetLayout layout;
        uint32_t count;

        // members in layout
        std::vector<VkDescriptorSetLayoutBinding> bindings;
    };
public:
    template <Type T, size_t N> struct ArrayDescriptor;
    template <size_t N> struct ArrayDescriptor<Type::Uniform, N> : public DescriptorBase {
        ArrayDescriptor(uint32_t bindPoint, VkShaderStageFlags stages) : DescriptorBase(bindPoint, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, N, stages) {}
    };
    template <size_t N> struct ArrayDescriptor<Type::ImageSampler, N> : public DescriptorBase {
        ArrayDescriptor(uint32_t bindPoint, VkShaderStageFlags stages) : DescriptorBase(bindPoint, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, N, stages) {}
    };
    template <size_t N> struct ArrayDescriptor<Type::StorageBuffer, N>  : public DescriptorBase {
        ArrayDescriptor(uint32_t bindPoint, VkShaderStageFlags stages) : DescriptorBase(bindPoint, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, N, stages) {}
    };
    using UniformDescriptor = ArrayDescriptor<Type::Uniform, 1>;
    using ImageSamplerDescriptor = ArrayDescriptor<Type::ImageSampler, 1>;
    using StorageBufferDescriptor = ArrayDescriptor<Type::StorageBuffer, 1>;

    typedef size_t DescriptorSetId;
public:
    class CompiledDescriptorSet {
    public:
        ~CompiledDescriptorSet() {
            if (m_pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device.Get(), m_pool, nullptr);
            }
            for (DescriptorSet::DescriptorSetDescription& desc : m_descriptions) {
                if (desc.layout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(m_device.Get(), desc.layout, nullptr);
                }
            }
        }

        CompiledDescriptorSet(CompiledDescriptorSet&& other) : m_device{ other.m_device }, m_pool{ other.m_pool } {
            other.m_pool = VK_NULL_HANDLE;
            
            std::swap(m_sets, other.m_sets);
            std::swap(m_descriptions, other.m_descriptions);
            assert(other.m_descriptions.size() == 0 && "Unexpected behaviour");
        }

        void UpdateUniformDescriptor(DescriptorSetId setId, uint32_t bindingId, VkBuffer buffer, uint32_t offset, size_t size) {
            VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = buffer;
			bufferInfo.offset = 0;
			bufferInfo.range = size;

            std::vector<VkDescriptorSet>& set = m_sets[setId];
            std::vector<VkWriteDescriptorSet> writes(set.size());
            std::transform(set.begin(), set.end(), writes.begin(), [pBufferInfo = &bufferInfo, bindingId](VkDescriptorSet& set) {
                VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
				write.dstSet  = set;
                write.dstBinding = bindingId;
				write.dstArrayElement = 0;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.pImageInfo = nullptr;
                write.pBufferInfo = pBufferInfo;
                write.pTexelBufferView = nullptr;

				return write;
            });

            vkUpdateDescriptorSets(m_device.Get(), writes.size(), writes.data(), 0, nullptr);
        }

        VkDescriptorSetLayout GetLayout(DescriptorSetId setId) const { 
            return m_descriptions[setId].layout; 
        }
    protected:
        friend DescriptorSet;
        CompiledDescriptorSet(VulkanDevice& device) : m_device(device) { } 
        VulkanDevice& m_device;

        // generated from `DescriptorSet`
        VkDescriptorPool m_pool;
        std::vector< std::vector< VkDescriptorSet > > m_sets;

        // transferred owner from `DescriptorSet` to `CompiledDescriptorSet`
        std::vector<DescriptorSet::DescriptorSetDescription> m_descriptions;
    };
    DescriptorSet(VulkanDevice &device) : m_device(device) { }

    ~DescriptorSet() {
        for (auto &set : m_descriptorSets) {
            vkDestroyDescriptorSetLayout(m_device.Get(), set.layout, nullptr);
        }
    }

    DescriptorSetId AddDescriptorSet(std::initializer_list<DescriptorBase> layout, uint32_t count) {
        { // check all components in layout have different binding locations
            std::set<uint32_t> pool;
            for (const DescriptorBase& descriptor : layout) {
                auto it = pool.find(descriptor.vkBinding.binding);
                if (it!= pool.end()) {
                    throw std::runtime_error("DescriptorSet: Duplicate binding");
                }
                else {
                    pool.insert(descriptor.vkBinding.binding);
                }
            }
        }

        DescriptorSet::DescriptorSetDescription& description = m_descriptorSets.emplace_back();
        std::transform(layout.begin(), layout.end(), std::back_inserter(description.bindings), [](const DescriptorBase& descriptor) { return descriptor.vkBinding; });

        description.count = count;

		VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0 };
        layoutInfo.bindingCount = static_cast<uint32_t>(description.bindings.size());
        layoutInfo.pBindings = description.bindings.data();
        
        if (VkResult result = vkCreateDescriptorSetLayout(m_device.Get(), &layoutInfo, nullptr, &description.layout); result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }

        return m_descriptorSets.size() - 1;
    }

    std::unique_ptr<CompiledDescriptorSet> Compile() {
        std::unique_ptr<CompiledDescriptorSet> ret{ new CompiledDescriptorSet{ m_device } };

        std::map<VkDescriptorType, uint32_t> typeCount;
        uint32_t maxSets = 0;
        for (DescriptorSetDescription& description : m_descriptorSets) {
            maxSets += description.count;
            for (VkDescriptorSetLayoutBinding& binding : description.bindings) {
                typeCount[binding.descriptorType] += description.count;
            }
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        for (auto &[type, count] : typeCount) {
            VkDescriptorPoolSize poolSize;
            poolSize.type = type;
            poolSize.descriptorCount = count;
            poolSizes.push_back(poolSize);
        }

		VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0 };
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxSets;
        
        if (VkResult result = vkCreateDescriptorPool(m_device.Get(), &poolInfo, nullptr, &ret->m_pool); result!= VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }

        // TODO: this can allocate all at once to boost performance
        for (DescriptorSetDescription& set : m_descriptorSets) {
            std::vector<VkDescriptorSet>& retSet = ret->m_sets.emplace_back();
            retSet.resize(set.count);

            std::vector<VkDescriptorSetLayout> layouts(set.count, set.layout);
            VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
            allocInfo.descriptorPool = ret->m_pool;
            allocInfo.descriptorSetCount = set.count;
            allocInfo.pSetLayouts = layouts.data();

            if (VkResult result = vkAllocateDescriptorSets(m_device.Get(), &allocInfo, retSet.data()); result != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate descriptor sets!");
            }
        }

        // transfer owner
        std::swap(ret->m_descriptions, m_descriptorSets);
        assert(m_descriptorSets.empty() && "All descriptors should be transferred");

        // this will call move constructor for sure, just remember don't destroy things twice
        // when user call `Compile()`, `DescriptorSet` can be safely destroyed or used as completely new one.
        return ret;
    }

protected:
    std::vector<DescriptorSetDescription> m_descriptorSets;
    VulkanDevice &m_device;
};

struct GraphicsPipelineConfig {
    class ShaderModule {
    public:
        enum class Type {
            Vertex,
            Tessellation,
            Geometry,
            Fragment,
            Compute
        };
        ShaderModule(Type type) : m_type(type) { }
        ~ShaderModule()  {
            Reset();
        }
        inline void Reset() {
            m_loaded = false;
        }
        inline bool Empty() const {
            return !m_loaded;
        }

        bool LoadFromFile(const std::string& filename, std::string entryName = "main") {
            m_binaryCode = readFile(filename);
            m_entryName = entryName;
            m_loaded = true;
            return m_loaded;
        }

    protected:
        friend class FrameGraph;
        Type m_type;
        std::vector<char> m_binaryCode;
        std::string m_entryName;
        bool m_loaded = false;
    private:
    	static std::vector<char> readFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }

            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();
            return buffer;
        }
    };

    class VertexInput {
    public:
        typedef uint32_t BindingId;
    protected:
        enum class AttributeFormat {
            Float = VK_FORMAT_R32_SFLOAT,
            Vec2 = VK_FORMAT_R32G32_SFLOAT,
            Vec3 = VK_FORMAT_R32G32B32_SFLOAT,
            Vec4 = VK_FORMAT_R32G32B32A32_SFLOAT,
            IVec2 = VK_FORMAT_R32G32_SINT,
            UVec4 = VK_FORMAT_R32G32B32A32_UINT,
            Double = VK_FORMAT_R64_SFLOAT
        };
        class AttributeBase {
            friend VertexInput;
        protected:
            constexpr AttributeBase(uint32_t location, AttributeFormat format, uint32_t offset) : m_location(location), m_format(format), m_offset(offset) { }
            AttributeFormat m_format;
            uint32_t m_location;
            uint32_t m_offset;
        };

        struct BindingDescription {
            BindingId id;
            VkVertexInputBindingDescription vkDescription;
            std::vector<VkVertexInputAttributeDescription> attributes;
        };
    public:
        struct Vec2Attribute : public AttributeBase {
            constexpr Vec2Attribute(uint32_t location, uint32_t offset) : AttributeBase(location, AttributeFormat::Vec2, offset) { }
        };
        struct Vec3Attribute  : public AttributeBase  {
            constexpr Vec3Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::Vec3, offset)  {  }
        };
        struct Vec4Attribute  : public AttributeBase  {
            constexpr Vec4Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::Vec4, offset)  {  }
        };
        struct IVec2Attribute  : public AttributeBase  {
            constexpr IVec2Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::IVec2, offset)  {  }
        };
        struct UVec4Attribute  : public AttributeBase  {
            constexpr UVec4Attribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::UVec4, offset)  {  }
        };
        struct DoubleAttribute  : public AttributeBase  {
            constexpr DoubleAttribute(uint32_t location, uint32_t offset)  : AttributeBase(location, AttributeFormat::Double, offset)  {  }
        };

        BindingId AddVertexAttributes(std::vector<AttributeBase> attributes, uint32_t strideOfAllAttributes) {
            BindingDescription& desc = m_bindings.emplace_back();
            desc.id = m_bindings.size() - 1;

            desc.vkDescription.binding = 0;
            desc.vkDescription.stride = strideOfAllAttributes;
            desc.vkDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            for (const AttributeBase& attribute : attributes) {
                VkVertexInputAttributeDescription vkAttribute = {};
                vkAttribute.binding = 0;
                vkAttribute.location = attribute.m_location;
                vkAttribute.format = static_cast<VkFormat>(attribute.m_format);
                vkAttribute.offset = attribute.m_offset;

                desc.attributes.push_back(vkAttribute);
            }

            return desc.id;
        }
        
    protected:
        friend class FrameGraph;
        std::vector<BindingDescription> m_bindings;
        VkPrimitiveTopology m_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    };

    struct VulkanDepth {
        MinMax bounds;
    };

    struct PipelineLayout {
        std::shared_ptr<DescriptorSet::CompiledDescriptorSet> descriptorSets;
        std::vector<DescriptorSet::DescriptorSetId> used;
    };

    ShaderModule vertexShader;
    ShaderModule tessellationShader;
    ShaderModule geometryShader;
    ShaderModule fragmentShader;
    VertexInput vertexInput;
    VulkanDepth depthStencil;
    PipelineLayout pipelineLayout;
    
    
    GraphicsPipelineConfig() :
        vertexShader{  ShaderModule::Type::Vertex },
        tessellationShader{  ShaderModule::Type::Tessellation },
        geometryShader{  ShaderModule::Type::Geometry },
        fragmentShader{  ShaderModule::Type::Fragment }
    {

    }
};

/**
 * Directed acyclic graph implemented using orthogonal list.
 */
class DAG {
public:
    static constexpr size_t EndOfList = std::numeric_limits<size_t>::max();
private:
    struct Arc {
        size_t headVertex, tailVertex;
        size_t headNextArc; // index of next arc in the same head vertex
        size_t tailNextArc; // index of next arc in the same tail vertex
    };
    struct Vertex {
        size_t firstInArc, firstOutArc;
        Vertex() : firstInArc(EndOfList), firstOutArc(EndOfList) {}
    };
public:
    DAG(size_t count): m_vertices(count) {

    }
    void AddEdge(size_t head, size_t tail) {
        Arc& arc = m_arcs.emplace_back();
        const size_t arcIndex = m_arcs.size() - 1;
        arc.headVertex = head;
        arc.tailVertex = tail;
        
        arc.headNextArc = m_vertices[head].firstOutArc;
        m_vertices[head].firstOutArc = arcIndex;

        arc.tailNextArc = m_vertices[tail].firstInArc;
        m_vertices[tail].firstInArc = arcIndex;
    }
    std::vector<size_t> QueryStartingVertices() const {
        std::vector<size_t> result;
        for (size_t i = 0; i < m_vertices.size(); ++i) {
            if (m_vertices[i].firstInArc == EndOfList) {
                result.emplace_back(i);
            }
        }
        return result;
    }
    std::vector<size_t> QueryEndingVertices() const {
        std::vector<size_t> result;
        for (size_t i = 0; i < m_vertices.size(); ++i) {
            if (m_vertices[i].firstOutArc == EndOfList) {
                result.emplace_back(i);
            }
        }
        return result;
    }
    std::vector<size_t> QueryNextArcs(size_t vertexIndex) const {
        if (vertexIndex >= m_vertices.size()) {
            throw std::out_of_range("Vertex index out of range");
        }

        std::vector<size_t> result;
        int current = m_vertices[vertexIndex].firstOutArc;
        for (; current != EndOfList; current = m_arcs[current].headNextArc) {
            result.emplace_back(m_arcs[current].tailVertex);
        }
        return result;
    }
    std::vector<size_t> QueryPrevArcs(size_t vertexIndex) const {
        if (vertexIndex >= m_vertices.size()) {
            throw std::out_of_range("vertex index out of range");
        }

        std::vector<size_t> result;
        int current = m_vertices[vertexIndex].firstInArc;
        for (; current != EndOfList; current = m_arcs[current].tailNextArc) {
            result.emplace_back(m_arcs[current].headVertex);
        }
        return result;
    }
private:
    std::vector<Vertex> m_vertices;
    std::vector<Arc> m_arcs;
};

class FrameGraph {
// struct SubpassDescription;
#pragma region Resources
public:
    enum class ResourceType {
        Color,
        Resolve,
        Depth
    };

    struct ResourceId {
        const ResourceType type;
        uint32_t index;
    protected:
        ResourceId(ResourceType type_, uint32_t index_) : type(type_), index(index_) {}
        friend class FrameGraph;
        friend struct SubpassDescription;
    };

    ResourceId AddColorResource(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE) {
        VkAttachmentDescription attachment{ 0 };
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.format = m_swapChain.GetFormat();
        attachment.samples = m_swapChain.GetSampleCount();
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        m_attachments.push_back(attachment);

        return ResourceId(ResourceType::Color, m_resources.size() - 1);
    }

    ResourceId AddResolveResource(VkAttachmentLoadOp loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE, VkAttachmentStoreOp storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE) {
        VkAttachmentDescription attachment{ 0 };
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.format = m_swapChain.GetFormat();
        attachment.samples = m_swapChain.GetSampleCount();
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        m_attachments.push_back(attachment);

        return ResourceId(ResourceType::Resolve, m_resources.size() - 1);
    }

    ResourceId AddDepthResource(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE, VkImageTiling tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL, VkFormatFeatureFlags features = 0) {
        VkAttachmentDescription attachment{ 0 };
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment.format = findDepthFormat(tiling, features | VkFormatFeatureFlagBits::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        attachment.samples = m_swapChain.GetSampleCount();
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        m_attachments.push_back(attachment);

        return ResourceId(ResourceType::Depth, m_resources.size() - 1);
    }

#pragma endregion

#pragma region Pipelines

public:
    //     uint32_t index;
    //     PipelineId(uint32_t _index) : index(_index) { }
    // };
    typedef uint32_t PipelineId;
    struct Pipeline {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
    };

    PipelineId AddGraphicsPipeline(GraphicsPipelineConfig config) {
        m_pipelineDescs.push_back(config);
        return m_pipelineDescs.size() - 1;
        // return PipelineId{ static_cast<uint32_t>(m_pipelineDescs.size()) - 1 };
    }

#pragma endregion

#pragma region Subpasses

public:
    // This enum derived from VkPipelineBindPoint so that it can be converted to VkPipelineBindPoint easily
    enum class SubpassType : std::underlying_type<VkPipelineBindPoint>::type {
        Graphics = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
        Compute = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    };

    typedef int SubpassId;

    SubpassId AddGraphicsSubpass(std::initializer_list<ResourceId> inputs, std::initializer_list<ResourceId> outputs, PipelineId pipeline, SubpassId previous = -1) {
        SubpassDescription& subpass = m_subpassDescs.emplace_back();
        subpass.inputResources = std::vector<ResourceId>(inputs);
        subpass.outputResources = std::vector<ResourceId>(outputs);
        subpass.previousPass = previous;
        subpass.type = SubpassType::Graphics;
        subpass.index = m_subpassDescs.size() - 1;
        subpass.pipeline = pipeline;

        return subpass.index;
    }

protected:
    struct SubpassDescription {
        std::vector<ResourceId> inputResources;
        std::vector<ResourceId> outputResources;
        SubpassId previousPass;
        SubpassType type;
        uint32_t index;

        PipelineId pipeline;
    };

#pragma endregion

public:
    FrameGraph(VulkanSwapChain &swapChain) : m_swapChain(swapChain) { }
    ~FrameGraph() {
        for (IVulkanImage *resource : m_resources) {
            delete resource;
        }

        for (Pipeline& pipeline : m_pipelines) {
            vkDestroyPipeline(m_swapChain.GetDevice().Get(), pipeline.pipeline, nullptr);
            vkDestroyPipelineLayout(m_swapChain.GetDevice().Get(), pipeline.layout, nullptr);
        }

        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_swapChain.GetDevice().Get(), m_renderPass, nullptr);
        }
    }

    void Build() {
        createRenderPass();
        createPipelines();
    }
private:
    void createRenderPass() {
        // transform m_subpasses into dag.
        DAG dag{ m_subpassDescs.size() };
        for (auto& subpass : m_subpassDescs) {
            if (subpass.previousPass != -1) {
                dag.AddEdge(subpass.previousPass, subpass.index);
            }
        }
        size_t startPassId = DAG::EndOfList, endPassId = DAG::EndOfList;
        if (std::vector<size_t> startPassIds = dag.QueryStartingVertices(); startPassIds.size() == 1) {
            startPassId = startPassIds[0];
        } else {
            throw std::runtime_error("zero or more than one starting pass. require only one starting pass.");
        }
        if (std::vector<size_t> endPassIds = dag.QueryStartingVertices(); endPassIds.size() == 1) {
            endPassId = endPassIds[0];
        } else {
            throw std::runtime_error("zero or more than one ending pass. require only one ending pass.");
        }

        // construct dependencies
        // https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Subpass-dependencies
        std::vector<VkSubpassDependency> vkDependencies;
        { // fill in vkDependencies
            { // add starting pass dependency
                SubpassDescription& startSubPass = m_subpassDescs[startPassId];
                VkSubpassDependency dependency{ 0 };
                dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
                dependency.dstSubpass = startPassId;
                dependency.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstStageMask = getSrcStageMask(startSubPass.inputResources);
                dependency.srcAccessMask = 0; // FIXME: ?
                dependency.dstAccessMask = getSrcAccessMask(startSubPass.inputResources);

                vkDependencies.push_back(dependency);
            }
            { // add ending pass dependency
                SubpassDescription& endSubPass = m_subpassDescs[endPassId];
                VkSubpassDependency dependency{ 0 };
                dependency.srcSubpass  = endPassId;
                dependency.dstSubpass  = VK_SUBPASS_EXTERNAL;
                dependency.srcStageMask  = getDstStageMask(endSubPass.outputResources);
                dependency.dstStageMask  = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.srcAccessMask  = getDstAccessMask(endSubPass.outputResources);
                dependency.dstAccessMask  = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                vkDependencies.push_back(dependency);
            }

            std::queue< std::pair<size_t, size_t> > searchQueue;
            std::vector<bool> visited(m_subpassDescs.size(), false);
            { // init search
                for (size_t target : dag.QueryNextArcs(startPassId)) {
                    searchQueue.push({ startPassId, target });
                }
                visited[startPassId] = true;
            }
            
            while (!searchQueue.empty()) { // search
                std::pair<size_t, size_t> edge = searchQueue.front();
                searchQueue.pop();

                SubpassDescription& fromSubpass = m_subpassDescs[edge.first];
                SubpassDescription& toSubpass   = m_subpassDescs[edge.second];

                VkSubpassDependency dependency{ 0 };
                dependency.srcSubpass = fromSubpass.index;
                dependency.dstSubpass = toSubpass.index;
                dependency.srcStageMask = getDstStageMask(fromSubpass.outputResources);
                dependency.dstStageMask = getSrcStageMask(toSubpass.inputResources);
                dependency.srcAccessMask = getDstAccessMask(fromSubpass.outputResources);
                dependency.dstAccessMask = getSrcAccessMask(toSubpass.inputResources);

                vkDependencies.push_back(dependency);

                if (visited[toSubpass.index] == false) {
                    for (size_t target : dag.QueryNextArcs(edge.second)) {
                        searchQueue.push({ edge.second, target });
                    }
                    visited[toSubpass.index] = true;
                }
            }
        }

        // construct descriptions
        struct SubpassDescriptionStorage {
            std::vector<VkAttachmentReference> colorAttachments;
            std::vector<VkAttachmentReference> inputAttachments;
            std::vector<uint32_t> preserveAttachments;
            std::optional<VkAttachmentReference> depthStencilAttachment;
            std::optional<VkAttachmentReference> resolveAttachment;
        };
        std::vector<SubpassDescriptionStorage> subpassDescriptionsStorage(m_subpassDescs.size());
        std::vector<VkSubpassDescription> subpassDescription(m_subpassDescs.size());
        for (size_t i = 0; i < m_subpassDescs.size(); ++i) {
            SubpassDescription& subpass = m_subpassDescs[i];
            SubpassDescriptionStorage& storage = subpassDescriptionsStorage[i];

            for (const ResourceId& resource : subpass.inputResources) {
                switch (resource.type) {
                    case ResourceType::Color: {
                        VkAttachmentReference reference{ 0 };
                        reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        reference.attachment = resource.index;

                        storage.colorAttachments.push_back(reference);
                        break;
                    }
                    case ResourceType::Resolve: {
                        VkAttachmentReference reference{ 0 };
                        reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        reference.attachment = resource.index;

                        if (storage.resolveAttachment.has_value()) {
                            throw std::runtime_error("Multiple resolve targets");
                        }
                        storage.resolveAttachment = reference;
                        break;
                    }
                    case ResourceType::Depth: {
                        VkAttachmentReference reference{ 0 };
                        reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        reference.attachment = resource.index;

                        if (storage.depthStencilAttachment.has_value()) {
                            throw std::runtime_error("Multiple depth targets");
                        }
                        storage.depthStencilAttachment = reference;
                    }
                    default:
                        throw std::runtime_error("Invalid resource type in subpass");
                }
            }
            for (const ResourceId& resource : subpass.outputResources) {
                switch (resource.type) {
                    case ResourceType::Color: {
                        VkAttachmentReference reference{ 0 };
                        reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        reference.attachment = resource.index;

                        storage.colorAttachments.push_back(reference);
                        break;
                    }
                    case ResourceType::Depth: {
                        VkAttachmentReference reference{ 0 };
                        reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                        reference.attachment = resource.index;

                        if (storage.depthStencilAttachment.has_value()) {
                            throw std::runtime_error("Multiple depth targets");
                        }
                        storage.depthStencilAttachment = reference;
                        break;
                    }
                    default:
                        throw std::runtime_error("Invalid resource type in subpass");
                }
            }

            VkSubpassDescription description{ 0 };
            description.colorAttachmentCount = static_cast<uint32_t>(storage.colorAttachments.size());
            description.pColorAttachments = (storage.colorAttachments.empty() ? nullptr : storage.colorAttachments.data());
            description.inputAttachmentCount = static_cast<uint32_t>(storage.inputAttachments.size());
            description.pInputAttachments   = (storage.inputAttachments.empty() ? nullptr : storage.inputAttachments.data());
            description.preserveAttachmentCount = static_cast<uint32_t>(storage.preserveAttachments.size());
            description.pPreserveAttachments    = (storage.preserveAttachments.empty()  ? nullptr : storage.preserveAttachments.data());
            description.pipelineBindPoint      = static_cast<VkPipelineBindPoint>(subpass.type);
            description.pDepthStencilAttachment = (storage.depthStencilAttachment.has_value() ? &storage.depthStencilAttachment.value() : nullptr);
            description.pResolveAttachments      = (storage.resolveAttachment.has_value() ? &storage.resolveAttachment.value() : nullptr);

            subpassDescription[i] = description;
        }

        // create render pass
        VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0 };
        renderPassInfo.attachmentCount   = static_cast<uint32_t>(m_attachments.size());
        renderPassInfo.pAttachments      = m_attachments.data();
        renderPassInfo.subpassCount        = static_cast<uint32_t>(subpassDescription.size());
        renderPassInfo.pSubpasses         = subpassDescription.data();
        renderPassInfo.dependencyCount    = static_cast<uint32_t>(vkDependencies.size());
        renderPassInfo.pDependencies      = vkDependencies.data();

        if (VkResult result = vkCreateRenderPass(m_swapChain.GetDevice().Get(), &renderPassInfo, nullptr, &m_renderPass); result!= VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass");
        }
    }
    
    // assumes m_renderPass is created and is not null
    void createPipelines() {
        m_pipelines.resize(m_pipelineDescs.size());
        for (size_t i = 0; i < m_pipelineDescs.size(); ++i) {
            m_pipelines[i] = createPipeline(m_pipelineDescs[i], i);
        }
    }
    // assumes m_renderPass is created and is not null
    Pipeline createPipeline(const GraphicsPipelineConfig& config, const PipelineId id) {
        Pipeline ret;

        #pragma region Vertex Input State
        std::vector<VkVertexInputBindingDescription> vertexBindings;
        std::vector<VkVertexInputAttributeDescription> vertexAttributes;
        for (const GraphicsPipelineConfig::VertexInput::BindingDescription& binding : config.vertexInput.m_bindings) {
            vertexBindings.push_back(binding.vkDescription);

            for (const VkVertexInputAttributeDescription& attribute : binding.attributes) {
                vertexAttributes.push_back(attribute);
            }
        }

        VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0 };
        vertexInput.vertexAttributeDescriptionCount = vertexAttributes.size();
        vertexInput.pVertexAttributeDescriptions = vertexAttributes.data();
        vertexInput.vertexBindingDescriptionCount = vertexBindings.size();
        vertexInput.pVertexBindingDescriptions = vertexBindings.data();
        #pragma endregion

        #pragma region Input Assembly State
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0 };
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        inputAssembly.topology = config.vertexInput.m_topology;
        #pragma endregion
        
        #pragma region Shaders
        std::vector<VkShaderModule> shaderModules;
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        constexpr VkPipelineShaderStageCreateInfo shaderStageDefaultTemplate{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0};
        if (!config.vertexShader.Empty()) {
            shaderModules.emplace_back(VK_NULL_HANDLE);
            shaderStages.push_back(shaderStageDefaultTemplate);
            createShaderModule(config.vertexShader, shaderModules.back(), shaderStages.back());
        }
        if (!config.fragmentShader.Empty()) {
            shaderModules.emplace_back(VK_NULL_HANDLE);
            shaderStages.push_back(shaderStageDefaultTemplate);
            createShaderModule(config.fragmentShader, shaderModules.back(), shaderStages.back());
        }
        #pragma endregion

        #pragma region Viewport State
        const static std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0 };
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        const VkExtent2D swapChainExtent{ m_swapChain.GetWidth(), m_swapChain.GetHeight() };
        VkViewport viewport{ 0.0f, 0.0f, (float)swapChainExtent.width, (float)swapChainExtent.height, 0.0f, 1.0f }; /* 视窗 */
        VkRect2D scissor{ {0, 0}, swapChainExtent }; /* 裁剪 */
        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0 };
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        #pragma endregion

        #pragma region Rasterizer State
        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0 };
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasSlopeFactor = 0.0f;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;
		rasterizer.polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
        #pragma endregion

        #pragma region Depth Stencil State
        VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0 };
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {};
		depthStencil.back = {};
        #pragma endregion

        #pragma region Multisample State
        VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0 };
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;
		multisampling.pSampleMask = nullptr;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.minSampleShading = .2f;
        #pragma endregion

        #pragma region Color Blend
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0 };
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
        #pragma endregion

        #pragma region Pipeline Layout
        std::vector<VkDescriptorSetLayout> descriptorSetLayout{ config.pipelineLayout.used.size() };
        for (size_t i = 0; i < config.pipelineLayout.used.size(); ++i) {
            descriptorSetLayout[i] = config.pipelineLayout.descriptorSets->GetLayout(i);
        }

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0 };
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
		pipelineLayoutInfo.setLayoutCount = descriptorSetLayout.size();
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayout.data();

		if (vkCreatePipelineLayout(m_swapChain.GetDevice().Get(), &pipelineLayoutInfo, nullptr, &ret.layout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
        #pragma endregion

        int subpassId = -1;
        if (auto found = std::find_if(m_subpassDescs.begin(), m_subpassDescs.end(), [id](const SubpassDescription& desc) {
            return desc.pipeline == id;
        }); found != m_subpassDescs.end()) {
            subpassId = found->index;
        } else {
            throw std::runtime_error("failed to find pipeline!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0 };
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;
		pipelineInfo.layout = ret.layout;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.stageCount = shaderStages.size();
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.renderPass = m_renderPass;
		pipelineInfo.subpass = subpassId;

        if (VkResult result = vkCreateGraphicsPipelines(m_swapChain.GetDevice().Get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &ret.pipeline); result != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		for (VkShaderModule shaderModule : shaderModules) {
            vkDestroyShaderModule(m_swapChain.GetDevice().Get(), shaderModule, nullptr);
        }
        return ret;
    }

    void createShaderModule(const GraphicsPipelineConfig::ShaderModule& sm, VkShaderModule &module, VkPipelineShaderStageCreateInfo &createInfo) {
        VkShaderModuleCreateInfo shaderCreateInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0 };
        shaderCreateInfo.codeSize = sm.m_binaryCode.size();
        shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(sm.m_binaryCode.data());

        if (vkCreateShaderModule(m_swapChain.GetDevice().Get(), &shaderCreateInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        createInfo.module = module;
        createInfo.pName = sm.m_entryName.c_str();
        createInfo.pSpecializationInfo = nullptr;
        switch (sm.m_type) {
            case GraphicsPipelineConfig::ShaderModule::Type::Vertex:
                createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case GraphicsPipelineConfig::ShaderModule::Type::Tessellation:
                throw std::runtime_error("tessellation not supported");
            case GraphicsPipelineConfig::ShaderModule::Type::Geometry:
                createInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
                break;
            case GraphicsPipelineConfig::ShaderModule::Type::Fragment:
                createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            default:
                throw std::runtime_error("unknown shader type");
        }
    }

protected:
    VulkanSwapChain& m_swapChain;

    // ===================   Descriptions  ======================
    std::vector< SubpassDescription > m_subpassDescs;
    std::vector< GraphicsPipelineConfig > m_pipelineDescs;
    std::vector< VkAttachmentDescription > m_attachments;

    // =====================   Storages   ======================
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    // raw vk handles inside. remember to destroy!
    std::vector< Pipeline > m_pipelines;

    // Notice: own, remember to destroy!
    // The very first resource is the swapchain image but here doesn't include it actually.
    // The swapchain image is implicitly included in the outputs.
    std::vector< IVulkanImage* > m_resources;

private:
    VkFormat findDepthFormat(VkImageTiling tiling, VkFormatFeatureFlags features) {
        std::array<VkFormat, 3> candidates = { 
            VK_FORMAT_D32_SFLOAT, 
            VK_FORMAT_D32_SFLOAT_S8_UINT, 
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (auto format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_swapChain.GetDevice().GetPhysicalDevice(), format, &props);
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("failed to find suitable depth format!");
    }

    VkPipelineStageFlags getSrcStageMask(const std::vector<ResourceId> &inputsOfPass) {
        if (inputsOfPass.empty()) {
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }

        VkPipelineStageFlags ret = 0;
        for (const ResourceId& resource : inputsOfPass) {
            switch(resource.type) {
                case ResourceType::Resolve:
                case ResourceType::Color: {
                    ret |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;
                }
                case ResourceType::Depth: {
                    ret |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // FIXME: ?
                    break;
                }
                default:
                    throw std::runtime_error("invalid resource type!");
            }
        }
        return ret;
    }
    VkPipelineStageFlags getDstStageMask(const std::vector<ResourceId> outputsOfPass) {
        if (outputsOfPass.empty()) {
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }

        VkPipelineStageFlags ret = 0;
        for (const ResourceId& resource : outputsOfPass) {
            switch (resource.type) {
                case ResourceType::Color: {
                    ret |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;
                }
                case ResourceType::Depth: {
                    ret |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // FIXME:  ?
                    break;
                }
                default:
                    throw std::runtime_error("invalid resource type");
            }
        }
        return ret;
    }
    VkAccessFlags getSrcAccessMask(const std::vector<ResourceId> &inputsOfPass) {
        if (inputsOfPass.empty()) {
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        }

        VkAccessFlags ret = 0;
        for (const ResourceId& resource : inputsOfPass) {
            switch (resource.type) {
                case ResourceType::Resolve:
                case ResourceType::Color: {
                    ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // FIXME: ?
                    break;
                }
                case ResourceType::Depth: {
                    ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // FIXME:  ?
                    break;
                }
                default:
                    throw std::runtime_error("invalid resource type");
            }
        }
        return ret;
    }
    VkAccessFlags getDstAccessMask(const std::vector<ResourceId> &outputsOfPass) {
        if (outputsOfPass.empty()) {
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        VkAccessFlags ret  = 0;
        for (const ResourceId& resource : outputsOfPass) {
            switch (resource.type) {
                case ResourceType::Color: {
                    ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // FIXME: ?
                    break;
                }
                case ResourceType::Depth: {
                    ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // FIXME: ?
                    break;
                }
                default:
                    throw std::runtime_error("invalid resource type");
            }
        }
        return ret;
    }
};


class Application {
public:
    Application() : 
        m_window{"Hello", 800, 600}, 
        m_instance{"Vulkan", m_window.GetRequiredExtensions()},
        m_surface{m_window.CreateSurface(m_instance.Get())},
        m_device{ m_instance, "discrete gpu:graphics,compute,present,swapchain,anisotropy,rate shading", &m_surface },
        m_swapChain{m_device},
        m_frameGraph{m_swapChain},
        m_descriptorLayout{m_device}
    {
        FrameGraph::ResourceId swapchain = m_frameGraph.AddColorResource(VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR);

        DescriptorSet::DescriptorSetId setId = m_descriptorLayout.AddDescriptorSet({
            DescriptorSet::UniformDescriptor(0, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT),
        }, m_swapChain.Count());
        std::unique_ptr<DescriptorSet::CompiledDescriptorSet> descriptorSet = m_descriptorLayout.Compile();

        GraphicsPipelineConfig config;
        config.vertexShader.LoadFromFile("shaders/shader.vert.spv");
        config.fragmentShader.LoadFromFile("shaders/shader.frag.spv");
        config.pipelineLayout.descriptorSets = std::move(descriptorSet);
        config.pipelineLayout.used = {setId};
        
        FrameGraph::PipelineId pipeline = m_frameGraph.AddGraphicsPipeline(config);

        FrameGraph::SubpassId subpass = m_frameGraph.AddGraphicsSubpass({}, {swapchain}, pipeline);

        m_frameGraph.Build();

        VulkanCommand::Builder builder;
        builder.AddCommandBuffer(m_device.GetGraphicsQueue(), 2);
        VulkanCommand vc = builder.Build(m_device);
        
    }

    ~Application() {
        
    }

    void run() {
        while (!m_window.ShouldClose()) {
            glfwPollEvents();
        }
    }
private: /* GLFW window */
    GLFWWindow m_window;

private: /* Vulkan */
    VulkanInstance m_instance;
    VulkanSurface m_surface;
    VulkanDevice m_device;
    VulkanSwapChain m_swapChain;
    FrameGraph m_frameGraph;

    DescriptorSet m_descriptorLayout;
};

int main() {
    Application app;
    app.run();
    return EXIT_SUCCESS;
}