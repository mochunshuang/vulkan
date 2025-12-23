

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <type_traits>
#include <utility>

#include "glfw.hpp"
#include "log.hpp"
#include "structure_chain.hpp"
#include "vulkan_utils.hpp"

#include "primitives.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <cassert>

#include <vulkan/vulkan_profiles.hpp>
#include <print>
#include <fstream>
#include <string>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

// Uniform Buffer对象结构 // NOLINTBEGIN
struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
}; // NOLINTEND

namespace vulkan
{

    struct debug_extension
    {
        // NOLINTNEXTLINE
        static constexpr auto VK_DEBUG_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

        static void addDebugExtension(
            std::vector<const char *> &required_instance_extensions)
        {
            required_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        [[nodiscard]] static bool addDebuglayerIfAvailable(
            std::vector<const char *> &requested_instance_layers,
            const std::vector<VkLayerProperties> &supported_instance_layers)
        {
            if (std::ranges::any_of(
                    supported_instance_layers, [](auto const &lp) noexcept {
                        return ::strcmp(lp.layerName, VK_DEBUG_LAYER_NAME) == 0;
                    }))
            {
                requested_instance_layers.push_back(VK_DEBUG_LAYER_NAME);
                LOGI("Enabled Validation Layer {}", VK_DEBUG_LAYER_NAME);
                return true;
            }
            LOGW("Validation Layer {} is not available", VK_DEBUG_LAYER_NAME);
            return false;
        }

        /// @brief A debug callback called from Vulkan validation layers.
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                      VkDebugUtilsMessageTypeFlagsEXT /*message_types*/,
                      const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                      void * /*user_data*/)
        {

            if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
            {
                LOGE("{} Validation Layer: Error: {}: {}", callback_data->messageIdNumber,
                     callback_data->pMessageIdName, callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
            {
                LOGW("{} Validation Layer: Warning: {}: {}",
                     callback_data->messageIdNumber, callback_data->pMessageIdName,
                     callback_data->pMessage);
            }
            else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) !=
                     0)
            {
                LOGI("{} Validation Layer: Information: {}: {}",
                     callback_data->messageIdNumber, callback_data->pMessageIdName,
                     callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0)
            {
                LOGD("{} Validation Layer: Verbose: {}: {}",
                     callback_data->messageIdNumber, callback_data->pMessageIdName,
                     callback_data->pMessage);
            }
            return VK_FALSE;
        }
        static constexpr void applyDebugCreateInfo(
            VkDebugUtilsMessengerCreateInfoEXT &debugCreateInfo)
        {
            debugCreateInfo.sType = vulkan::sType<VkDebugUtilsMessengerCreateInfoEXT>();
            debugCreateInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = &debugCallback;
        }
        void destroyDebugMessenger(VkInstance &instance) noexcept
        {
            if ((debug_ext != nullptr) && (instance != nullptr))
            { // NOLINTNEXTLINE
                auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    ::vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (func != nullptr)
                {
                    func(instance, debug_ext, nullptr);
                }
                debug_ext = VK_NULL_HANDLE;
            }
        }
        VkDebugUtilsMessengerEXT debug_ext = VK_NULL_HANDLE; // NOLINT
    };

    template <typename window_type>
    struct surface_extension
    {
        static void addSurfaceExtension(
            std::vector<const char *> &required_instance_extensions)
        {
            required_instance_extensions.append_range(
                window_type::getRequiredSurfaceExtensions());
        }
    };

    struct vulkan_instance : debug_extension
    {
        [[nodiscard]] static constexpr auto availableInstanceExtension()
            -> std::vector<VkExtensionProperties>
        {
            // NOTE: Properties 只读的意思
            uint32_t instance_extension_count; // NOLINT
            checkVkResult(::vkEnumerateInstanceExtensionProperties(
                nullptr, &instance_extension_count, nullptr));

            std::vector<VkExtensionProperties> available_instance_extensions(
                instance_extension_count);
            checkVkResult(::vkEnumerateInstanceExtensionProperties(
                nullptr, &instance_extension_count,
                available_instance_extensions.data()));
            return available_instance_extensions;
        }

        [[nodiscard]] static bool checkExtensionSupport(
            const std::vector<const char *> &required,
            const std::vector<VkExtensionProperties> &available =
                availableInstanceExtension())
        {
            for (const auto *extension_name : required)
            {
                bool found = std::ranges::any_of(
                    available, [&](auto const &available_extension) noexcept {
                        return ::strcmp(available_extension.extensionName,
                                        extension_name) == 0;
                    });
                if (!found)
                {
                    // Output an error message for the missing extension
                    LOGE("Required extension not found: {}", extension_name);
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static constexpr auto availableInstanceLayer()
            -> std::vector<VkLayerProperties>
        {
            uint32_t instance_layer_count; // NOLINT
            checkVkResult(
                ::vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

            std::vector<VkLayerProperties> supported_instance_layers(
                instance_layer_count);
            checkVkResult(::vkEnumerateInstanceLayerProperties(
                &instance_layer_count, supported_instance_layers.data()));
            return supported_instance_layers;
        }

        // prinnt
        static void printAvailableInstanceExtension()
        {
            std::print("AvailableInstanceExtension: [ ");
            for (int i = 0; const auto &e : availableInstanceExtension())
            {
                if (i == 0)
                {
                    std::print("{}", e.extensionName);
                    ++i;
                }
                else
                    std::print(", {}", e.extensionName);
            }
            std::print(" ].\n");
        }
        //-------------------------
        void teardownInstance() noexcept
        {
            // Cleanup debug messenger
            destroyDebugMessenger(instance);
            // Cleanup instance
            if (instance != nullptr)
            {
                ::vkDestroyInstance(instance, nullptr);
                instance = VK_NULL_HANDLE;
            }
        }

        VkInstance instance = VK_NULL_HANDLE; // NOLINT
    };

    struct physical_device_function_set
    {
        // ---------------------cpp like----------------------------------
        [[nodiscard]] static auto enumeratePhysicalDevices(VkInstance &instance)
        {
            uint32_t deviceCount; // NOLINT
            ::vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
            if (deviceCount == 0)
                throw std::runtime_error("failed to find GPUs with Vulkan support!");
            std::vector<VkPhysicalDevice> gpus(deviceCount);
            ::vkEnumeratePhysicalDevices(instance, &deviceCount, gpus.data());
            return gpus;
        }
        static auto getPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice &gpu)
        {
            uint32_t queueFamilyCount = 0;
            ::vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            ::vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount,
                                                       queueFamilies.data());
            return queueFamilies;
        }
        static auto enumerateDeviceExtensionProperties(const VkPhysicalDevice &gpu)
        {
            // Check if all required device extensions are available
            uint32_t extensionCount; // NOLINT
            ::vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount,
                                                   nullptr);
            std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
            ::vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount,
                                                   availableDeviceExtensions.data());
            return availableDeviceExtensions;
        }
        static auto getPhysicalDeviceProperties(const VkPhysicalDevice &gpu) noexcept
        {
            VkPhysicalDeviceProperties deviceProperties;
            ::vkGetPhysicalDeviceProperties(gpu, &deviceProperties);
            return deviceProperties;
        }
        static auto getPhysicalDeviceFormatProperties(const VkPhysicalDevice &gpu,
                                                      VkFormat format) noexcept
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(gpu, format, &formatProperties);
            return formatProperties;
        }
        [[nodiscard]] static auto getPhysicalDeviceSurfaceSupportKHR(
            VkPhysicalDevice &physicalDevice, uint32_t queueFamilyIndex,
            VkSurfaceKHR surface) noexcept
        {
            VkBool32 presentSupport; // NOLINT
            ::vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex,
                                                   surface, &presentSupport);
            return presentSupport;
        }
    };
    struct app_info // Application info to store profile support
    {
        bool profileSupported = false; // NOLINT
        VpProfileProperties profile;   // NOLINT

        // NOTE: 2. 检查是否支持，我们要求的特性
        void checkFeatureSupport(const VkInstance &instance,
                                 const VkPhysicalDevice &physicalDevice)
        {
            // NOTE: 根据 appInfo.profileSupported 来创建和配置
            // Check for Vulkan profile support
            VpProfileProperties profileProperties = {
                .profileName = VP_KHR_ROADMAP_2022_NAME,
                .specVersion = VP_KHR_ROADMAP_2022_SPEC_VERSION};

            VkBool32 supported; // NOLINT
            VkResult vk_result = ::vpGetPhysicalDeviceProfileSupport(
                instance, physicalDevice, &profileProperties, &supported);
            if (vk_result == VkResult::VK_SUCCESS && supported == VK_TRUE)
            {
                profileSupported = true; // TODO(mcs): 如果不支持应该回退
                profile = profileProperties;
                LOGI("Device supports Vulkan profile: {}", profileProperties.profileName);
            }
            else
            {
                LOGW("Device does not support Vulkan profile: {}",
                     profileProperties.profileName);
            }
        }
    };

    struct vulkan_context : vulkan_instance,
                            surface_extension<glfw::Window>,
                            physical_device_function_set
    {
        using window_type = glfw::Window;

        // NOLINTBEGIN
        VkSurfaceKHR surface_ext = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueIndex = ~0;
        VkQueue queue = VK_NULL_HANDLE;

        VkCommandPool commandPool = VK_NULL_HANDLE;

        app_info appInfo = {};

        //---------------------------------
        void createInstance(VkApplicationInfo &appInfo)
        {
            // c1: 1. extensions
            auto available_instance_extensions =
                vulkan_instance::availableInstanceExtension();
            std::vector<const char *> required_instance_extensions{};
            surface_extension::addSurfaceExtension(required_instance_extensions);
            debug_extension::addDebugExtension(required_instance_extensions);
            if (!vulkan_instance::checkExtensionSupport(required_instance_extensions,
                                                        available_instance_extensions))
                throw std::runtime_error("Required instance extensions are missing.");

            // c1: 2. layers
            auto supported_instance_layers = vulkan_instance::availableInstanceLayer();
            std::vector<const char *> requested_instance_layers{};
            bool has_debug_utils = debug_extension::addDebuglayerIfAvailable(
                requested_instance_layers, supported_instance_layers);
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
            std::vector<const char *> validationLayers = {};
            if (enableValidationLayers && has_debug_utils)
            {
                validationLayers.emplace_back(VK_DEBUG_LAYER_NAME);
                debug_extension::applyDebugCreateInfo(debugCreateInfo);
            }

            static_assert(vulkan::sType<VkInstanceCreateInfo>() ==
                          VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);

            // NOTE: 必须在创建的时候关联 扩展，才启动扩展
            VkInstanceCreateInfo createInfo = {
                .sType = vulkan::sType<VkInstanceCreateInfo>(),
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
                .ppEnabledLayerNames = validationLayers.data(),
                .enabledExtensionCount =
                    static_cast<uint32_t>(required_instance_extensions.size()),
                .ppEnabledExtensionNames = required_instance_extensions.data(),
            };
            // set debuger
            if (enableValidationLayers && has_debug_utils)
                createInfo.pNext = &debugCreateInfo; // c1: create with Debug

            // c1: 3. CreateInstance
            checkVkResult(::vkCreateInstance(&createInfo, nullptr, &instance));
        }

        constexpr void createSurface(window_type &window)
        {
            window.createVkSurfaceKHR(instance, surface_ext);
        }

        void pickPhysicalDevice(const std::vector<const char *> &requiredDeviceExtension)
        {
            std::vector<VkPhysicalDevice> gpus = enumeratePhysicalDevices(instance);

            const auto k_iter = std::ranges::find_if(gpus, [&](auto const &gpu) noexcept {
                // Check queue family support
                std::vector<VkQueueFamilyProperties> queueFamilies =
                    getPhysicalDeviceQueueFamilyProperties(gpu);
                bool supportsGraphics = std::ranges::any_of(
                    queueFamilies, [](auto const &queueFamily) noexcept {
                        return (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                    }); // NOTE: 这里查找可以变量输入

                // Check if all required device extensions are available
                std::vector<VkExtensionProperties> availableDeviceExtensions =
                    enumerateDeviceExtensionProperties(gpu);

                bool supportsAllRequiredExtensions = std::ranges::all_of(
                    requiredDeviceExtension,
                    [&availableDeviceExtensions](
                        auto const &requiredDeviceExtension) noexcept {
                        return std::ranges::any_of(
                            availableDeviceExtensions,
                            [&](auto const &availableDeviceExtension) noexcept {
                                return ::strcmp(availableDeviceExtension.extensionName,
                                                requiredDeviceExtension) == 0;
                            });
                    });
                return supportsGraphics && supportsAllRequiredExtensions;
            });
            if (k_iter != gpus.end())
            {
                physicalDevice = *k_iter;

                VkPhysicalDeviceProperties deviceProperties =
                    getPhysicalDeviceProperties(physicalDevice);
                LOGI("Selected GPU: {}. API Version: {}.{}.{}",
                     deviceProperties.deviceName,
                     VK_VERSION_MAJOR(deviceProperties.apiVersion),
                     VK_VERSION_MINOR(deviceProperties.apiVersion),
                     VK_VERSION_PATCH(deviceProperties.apiVersion));

                appInfo.checkFeatureSupport(instance,
                                            physicalDevice); // c2: profies check
                return;
            }
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        void createLogicalDevice(const std::vector<const char *> &requiredDeviceExtension)
        {

            std::vector<VkQueueFamilyProperties> queueFamilies =
                getPhysicalDeviceQueueFamilyProperties(physicalDevice);

            for (uint32_t i = 0, queueFamilyCount = queueFamilies.size();
                 i < queueFamilyCount; i++)
            {
                if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U)
                {
                    if (getPhysicalDeviceSurfaceSupportKHR(physicalDevice, i,
                                                           surface_ext) != 0U)
                    {
                        queueIndex = i;
                        break;
                    }
                }
            }
            if (queueIndex == ~0)
                throw std::runtime_error(
                    "Could not find a queue for graphics and present -> terminating");

            float queuePriority = 1.0F;
            VkDeviceQueueCreateInfo queueCreateInfo = {
                .sType = vulkan::sType<VkDeviceQueueCreateInfo>(),
                .queueFamilyIndex = queueIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority};

            vulkan::structure_chain<VkPhysicalDeviceFeatures2,
                                    VkPhysicalDeviceVulkan11Features,
                                    VkPhysicalDeviceVulkan13Features,
                                    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
                featureChain = {
                    // C9:  检查是否支持各向异性采样。纹理映射需要
                    {.features = {.samplerAnisotropy = VK_TRUE}},
                    {.shaderDrawParameters = VK_TRUE},
                    {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                    {.extendedDynamicState = VK_TRUE}};

            static_assert(std::is_same_v<decltype(auto(featureChain.head())),
                                         VkPhysicalDeviceFeatures2>);
            assert(featureChain.size() == requiredDeviceExtension.size());

            VkDeviceCreateInfo createInfo = {
                .sType = vulkan::sType<VkDeviceCreateInfo>(),
                .pNext = &featureChain.head(),
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &queueCreateInfo,
                .enabledExtensionCount =
                    static_cast<uint32_t>(requiredDeviceExtension.size()),
                .ppEnabledExtensionNames = requiredDeviceExtension.data(),
            };

            if (::vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create logical device!");
            ::vkGetDeviceQueue(device, queueIndex, 0, &queue);
        }

        void createCommandPool()
        {
            VkCommandPoolCreateInfo poolInfo = {
                .sType = vulkan::sType<VkCommandPoolCreateInfo>(),
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queueIndex};

            if (::vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to create command pool!");
            }
        }
        static VkFormat findSupportedFormat(VkPhysicalDevice &physicalDevice,
                                            const std::vector<VkFormat> &candidates,
                                            VkImageTiling tiling,
                                            VkFormatFeatureFlags features)
        {
            /*
            格式的支持取决于平铺模式和用法:
                linearTilingFeatures：线性平铺支持的用例
                optimalTilingFeatures：最佳平铺支持的用例
                bufferFeatures：缓冲区支持的用例

            */
            auto formatIt = std::ranges::find_if(candidates, [&](auto const format) {
                VkFormatProperties props =
                    vulkan_context::getPhysicalDeviceFormatProperties(physicalDevice,
                                                                      format);
                return (((tiling == VkImageTiling::VK_IMAGE_TILING_LINEAR) &&
                         ((props.linearTilingFeatures & features) == features)) ||
                        ((tiling == VkImageTiling::VK_IMAGE_TILING_OPTIMAL) &&
                         ((props.optimalTilingFeatures & features) == features)));
            });
            if (formatIt == candidates.end())
            {
                throw std::runtime_error("failed to find supported format!");
            }
            return *formatIt;
        }

      public:
        static auto findDepthFormat(VkPhysicalDevice &physicalDevice)
        {
            /*
            VK_FORMAT_D32_SFLOAT： 32位浮点深度
            VK_FORMAT_D32_SFLOAT_S8_UINT： 32位符号浮点深度和8位模板组件
            VK_FORMAT_D24_UNORM_S8_UINT： 24位浮点深度和8位模板组件
            */
            // 模板组件用于模板测试，这是一个额外的测试，可以与深度测试相结合。
            return findSupportedFormat(
                physicalDevice,
                {VkFormat::VK_FORMAT_D32_SFLOAT, VkFormat::VK_FORMAT_D32_SFLOAT_S8_UINT,
                 VkFormat::VK_FORMAT_D24_UNORM_S8_UINT},
                VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
                VkFormatFeatureFlagBits::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }
        void setup(VkApplicationInfo &appInfo, window_type &window)
        {
            createInstance(appInfo);
            createSurface(window);
            std::vector<const char *> requiredDeviceExtension = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
                VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}; // NOLINTEND

            pickPhysicalDevice(requiredDeviceExtension);
            createLogicalDevice(requiredDeviceExtension);
            createCommandPool();
        }
        void teardown()
        {
            // Cleanup command buffers and pool
            if (commandPool != nullptr)
            {
                ::vkDestroyCommandPool(device, commandPool, nullptr);
                commandPool = VK_NULL_HANDLE;
            }

            // Cleanup surface
            if ((surface_ext != nullptr) && (instance != nullptr))
            {
                ::vkDestroySurfaceKHR(instance, surface_ext, nullptr);
                surface_ext = VK_NULL_HANDLE;
            }

            // Cleanup device
            if (device != nullptr)
            {
                ::vkDestroyDevice(device, nullptr);
                device = VK_NULL_HANDLE;
            }

            teardownInstance();
        }
    };

    // c11: 深度图片
    struct deep_image
    {
        // NOLINTBEGIN
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE; // NOLINTEND

      private:
        void createDepthResources(vulkan_context &context, VkExtent2D &swapChainExtent,
                                  VkSampleCountFlagBits msaaSamples)
        {
            auto &physicalDevice = context.physicalDevice;
            auto &device = context.device;

            const auto k_depth_format = vulkan_context::findDepthFormat(physicalDevice);

            // 创建纹理图像
            VkImageCreateInfo imageCreateInfo{
                .sType = sType<VkImageCreateInfo>(),
                .imageType = VK_IMAGE_TYPE_2D,
                .format = k_depth_format,
                .extent = {.width = swapChainExtent.width,
                           .height = swapChainExtent.height,
                           .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = msaaSamples,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage =
                    VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
            createImage(physicalDevice, device, this->image, this->imageMemory,
                        imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            VkImageViewCreateInfo viewInfo{
                .sType = sType<VkImageViewCreateInfo>(),
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = k_depth_format,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            createImageView(device, imageView, viewInfo);
        }

      public:
        void setup(vulkan_context &context, VkExtent2D &swapChainExtent,
                   VkSampleCountFlagBits msaaSamples)
        {
            createDepthResources(context, swapChainExtent, msaaSamples);
        }
        void teardown(VkDevice &device)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
            if (image != VK_NULL_HANDLE)
            {
                vkDestroyImage(device, image, nullptr);
                image = VK_NULL_HANDLE;
            }
            if (imageMemory != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, imageMemory, nullptr);
                imageMemory = VK_NULL_HANDLE;
            }
        }
    };
    // c15: MSAA图片
    struct color_image
    {
        // NOLINTBEGIN
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE; // NOLINTEND

      private:
        void createColorResources(vulkan_context &context, VkExtent2D &swapChainExtent,
                                  VkFormat colorFormat, VkSampleCountFlagBits msaaSamples)
        {
            auto &physicalDevice = context.physicalDevice;
            auto &device = context.device;

            // 创建纹理图像
            VkImageCreateInfo imageCreateInfo{
                .sType = sType<VkImageCreateInfo>(),
                .imageType = VK_IMAGE_TYPE_2D,
                .format = colorFormat,
                .extent = {.width = swapChainExtent.width,
                           .height = swapChainExtent.height,
                           .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = msaaSamples,
                .tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
                .usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                         VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
            createImage(physicalDevice, device, this->image, this->imageMemory,
                        imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            VkImageViewCreateInfo viewInfo{
                .sType = sType<VkImageViewCreateInfo>(),
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = colorFormat,
                .subresourceRange = {.aspectMask =
                                         VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            createImageView(device, imageView, viewInfo);
        }

      public:
        void setup(vulkan_context &context, VkExtent2D &swapChainExtent,
                   VkFormat colorFormat, VkSampleCountFlagBits msaaSamples)
        {
            createColorResources(context, swapChainExtent, colorFormat, msaaSamples);
        }
        void teardown(VkDevice &device)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
            if (image != VK_NULL_HANDLE)
            {
                vkDestroyImage(device, image, nullptr);
                image = VK_NULL_HANDLE;
            }
            if (imageMemory != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, imageMemory, nullptr);
                imageMemory = VK_NULL_HANDLE;
            }
        }
    };

    struct presentation
    {
        // NOLINTBEGIN
        VkSampleCountFlagBits msaaSamples{};

        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        std::vector<VkImage> swapChainImages;
        VkSurfaceFormatKHR swapChainSurfaceFormat{};
        VkExtent2D swapChainExtent{};
        std::vector<VkImageView> swapChainImageViews; // NOLINTEND

        color_image colorImage{}; // NOLINT
        deep_image deepImage{};   // NOLINT

        vulkan_context *context{}; // NOLINT
        using window_type = vulkan_context::window_type;
        window_type *window{}; // NOLINT

      private:
        static uint32_t chooseSwapMinImageCount(
            const VkSurfaceCapabilitiesKHR &capabilities)
        {
            assert(capabilities.maxImageCount != 0);
            uint32_t minImageCount = capabilities.minImageCount + 1;
            if (capabilities.maxImageCount > 0 &&
                minImageCount > capabilities.maxImageCount)
                minImageCount = capabilities.maxImageCount;
            return minImageCount;
        }
        static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR> &availableFormats)
        {
            VkSurfaceFormatKHR select = {.format = VK_FORMAT_B8G8R8A8_SRGB,
                                         .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
            for (const auto &availableFormat : availableFormats)
            {
                if (availableFormat.format == select.format &&
                    availableFormat.colorSpace == select.colorSpace)
                    return availableFormat;
            }
            return availableFormats[0];
        }
        static VkPresentModeKHR chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR> &availablePresentModes)
        {
            VkPresentModeKHR select = VK_PRESENT_MODE_MAILBOX_KHR;
            for (const auto &availablePresentMode : availablePresentModes)
            {
                if (availablePresentMode == select)
                {
                    return availablePresentMode;
                }
            }
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        static VkExtent2D chooseSwapExtent(window_type &window,
                                           const VkSurfaceCapabilitiesKHR &capabilities)
        {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
                return capabilities.currentExtent;

            auto [width, height] = window.getFramebufferSize();
            return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width),
                    std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height)};
        }

        //-------------------------cpp like api------------------------------
        constexpr static auto getPhysicalDeviceSurfaceCapabilitiesKHR(
            VkPhysicalDevice &physicalDevice, VkSurfaceKHR &surface) noexcept
        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                        &surfaceCapabilities);
            return surfaceCapabilities;
        }
        constexpr static auto getPhysicalDeviceSurfaceFormatsKHR(
            VkPhysicalDevice &physicalDevice, VkSurfaceKHR &surface)
        {
            uint32_t formatCount; // NOLINT
            checkVkResult(::vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                                 &formatCount, nullptr));
            std::vector<VkSurfaceFormatKHR> formats(formatCount);
            checkVkResult(::vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice, surface, &formatCount, formats.data()));
            return formats;
        }
        constexpr static auto getPhysicalDeviceSurfacePresentModesKHR(
            VkPhysicalDevice &physicalDevice, VkSurfaceKHR &surface)
        {
            uint32_t presentModeCount; // NOLINT
            checkVkResult(::vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, nullptr));
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            checkVkResult(::vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, presentModes.data()));
            return presentModes;
        }
        constexpr static auto getSwapchainImagesKHR(VkDevice &device,
                                                    VkSwapchainKHR &swapchain)
        {
            uint32_t imageCount; // NOLINT
            std::vector<VkImage> associatedImages;
            ::vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
            associatedImages.resize(imageCount);
            ::vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                      associatedImages.data());
            return associatedImages;
        }
        //-------------------------------------------------------------------

        void createSwapChain()
        {
            auto &physicalDevice = context->physicalDevice;
            auto &surface = context->surface_ext;
            auto &device = context->device;
            auto &window = *(this->window);

            VkSurfaceCapabilitiesKHR surfaceCapabilities =
                getPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface);

            std::vector<VkSurfaceFormatKHR> availableFormats =
                getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface);

            std::vector<VkPresentModeKHR> availablePresentModes =
                getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface);

            swapChainExtent = chooseSwapExtent(window, surfaceCapabilities);
            swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
            uint32_t imageCount = chooseSwapMinImageCount(surfaceCapabilities);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

            VkSwapchainCreateInfoKHR createInfo = {
                .sType = vulkan::sType<VkSwapchainCreateInfoKHR>(),
                .surface = surface,
                .minImageCount = imageCount,
                .imageFormat = swapChainSurfaceFormat.format,
                .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                .imageExtent = swapChainExtent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .preTransform = surfaceCapabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = presentMode,
                .clipped = VK_TRUE};

            if (::vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create swap chain!");

            swapChainImages = getSwapchainImagesKHR(device, swapChain);
        }

        void createImageViews()
        {
            auto &device = context->device;

            swapChainImageViews.resize(swapChainImages.size());
            for (size_t i = 0; i < swapChainImages.size(); i++)
            {
                VkImageViewCreateInfo createInfo = {
                    .sType = vulkan::sType<VkImageViewCreateInfo>(),
                    .image = swapChainImages[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapChainSurfaceFormat.format,
                    .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                   .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                   .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                   .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1}};

                if (::vkCreateImageView(device, &createInfo, nullptr,
                                        &swapChainImageViews[i]) != VK_SUCCESS)
                    throw std::runtime_error("failed to create image views!");
            }
        }
        void cleanupSwapChain()
        {
            auto &device = context->device;

            // deep image
            if (context->device != nullptr)
            {
                deepImage.teardown(device);
                colorImage.teardown(device);
            }

            // 销毁图像视图
            for (auto *imageView : swapChainImageViews)
                ::vkDestroyImageView(device, imageView, nullptr);
            swapChainImageViews.clear();

            // 销毁交换链（这会自动销毁相关的VkImage）
            if (swapChain != nullptr)
            {
                ::vkDestroySwapchainKHR(device, swapChain, nullptr);
                swapChain = VK_NULL_HANDLE;
            }
        }
        [[nodiscard]] static VkSampleCountFlagBits getMaxUsableSampleCount(
            const VkPhysicalDevice &physicalDevice) noexcept
        {
            VkPhysicalDeviceProperties physicalDeviceProperties =
                vulkan_context::getPhysicalDeviceProperties(physicalDevice);
            auto counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                          physicalDeviceProperties.limits.framebufferDepthSampleCounts;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_64_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_64_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_32_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_32_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_16_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_16_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT;
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        }

      public:
        void setup(vulkan_context &ctx, window_type &w)
        {
            context = &ctx;
            window = &w;
            msaaSamples = getMaxUsableSampleCount(ctx.physicalDevice);
            createSwapChain();
            createImageViews();

            // c15: colorImage 在 deepImage 之前
            colorImage.setup(ctx, swapChainExtent, swapChainSurfaceFormat.format,
                             msaaSamples);
            deepImage.setup(ctx, swapChainExtent, msaaSamples);
        }
        void teardown() noexcept
        {
            cleanupSwapChain();
        }
        void recreateSwapChain()
        {
            auto &device = context->device;
            window->waitGoodFramebufferSize();
            ::vkDeviceWaitIdle(device);

            cleanupSwapChain();

            createSwapChain();
            createImageViews();
            colorImage.setup(*context, swapChainExtent, swapChainSurfaceFormat.format,
                             msaaSamples);
            deepImage.setup(*context, swapChainExtent, msaaSamples);
        }
    };

    struct descriptor_resource
    {
        // 静态常量
        static constexpr size_t MAX_OBJECT_COUNT = 12; // 增加对象数量 // NOLINT

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;           // NOLINT
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE; // NOLINT

        void setup(VkDevice &device, size_t maxSwapChainImages)
        {
            createDescriptorPool(device, MAX_OBJECT_COUNT, maxSwapChainImages);
            createDescriptorSetLayout(device);
        }

        // NOTE: 大小可以静态期确定吧：descriptorSetLayout消费descriptorPool
        void createDescriptorPool(VkDevice &device, size_t maxObjects,
                                  size_t maxSwapChainImages)
        {

            std::array poolSizes = {
                VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     .descriptorCount = static_cast<uint32_t>(
                                         maxObjects * maxSwapChainImages)},
                VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     .descriptorCount = static_cast<uint32_t>(
                                         maxObjects * maxSwapChainImages)}};

            VkDescriptorPoolCreateInfo poolInfo = {
                .sType = sType<VkDescriptorPoolCreateInfo>(),
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                .maxSets = static_cast<uint32_t>(maxObjects * maxSwapChainImages),
                // failed to allocate descriptor sets!
                //  .maxSets = static_cast<uint32_t>(2 * maxSwapChainImages):
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data()};

            if (::vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to create descriptor pool!");
            }
        }

        void createDescriptorSetLayout(VkDevice &device)
        {
            std::array bindings = {
                VkDescriptorSetLayoutBinding{.binding = 0,
                                             .descriptorType =
                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             .descriptorCount = 1,
                                             .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                             .pImmutableSamplers = nullptr},
                VkDescriptorSetLayoutBinding{
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .pImmutableSamplers = nullptr}};

            VkDescriptorSetLayoutCreateInfo layoutInfo = {
                .sType = sType<VkDescriptorSetLayoutCreateInfo>(),
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data()};

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                            &descriptorSetLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create descriptor set layout!");
            }
        }

        std::vector<VkDescriptorSet> allocateDescriptorSets(VkDevice &device,
                                                            size_t swapChainSize)
        {

            std::vector<VkDescriptorSetLayout> layouts(swapChainSize,
                                                       descriptorSetLayout);

            VkDescriptorSetAllocateInfo allocInfo = {
                .sType = sType<VkDescriptorSetAllocateInfo>(),
                .descriptorPool = descriptorPool,
                .descriptorSetCount = static_cast<uint32_t>(swapChainSize),
                .pSetLayouts = layouts.data()};

            std::vector<VkDescriptorSet> descriptorSets(swapChainSize);
            if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }

            return descriptorSets;
        }

        void teardown(VkDevice &device)
        {
            if (descriptorSetLayout != VK_NULL_HANDLE)
            {
                ::vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                descriptorSetLayout = VK_NULL_HANDLE;
            }
            if (descriptorPool != VK_NULL_HANDLE)
            {
                ::vkDestroyDescriptorPool(device, descriptorPool, nullptr);
                descriptorPool = VK_NULL_HANDLE;
            }
        }
    };
    struct graphics_pipeline
    {
        descriptor_resource descriptor_resource{};        // NOLINT
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE; // NOLINT
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;     // NOLINT
        vulkan_context *context{};                        // NOLINT

      private:
        static std::vector<char> readFile(const std::string &filename)
        {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);
            if (!file.is_open())
            {
                throw std::runtime_error("failed to open file!");
            }
            std::vector<char> buffer(file.tellg());
            file.seekg(0, std::ios::beg);
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            file.close();
            return buffer;
        }
        [[nodiscard]] VkShaderModule createShaderModule(
            const std::vector<char> &code) const
        {
            auto &device = context->device;

            VkShaderModuleCreateInfo createInfo = {
                .sType = vulkan::sType<VkShaderModuleCreateInfo>(),
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t *>(code.data())}; // NOLINT

            VkShaderModule shaderModule; // NOLINT
            if (::vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create shader module!");

            return shaderModule;
        }

        void createGraphicsPipeline(const VkSurfaceFormatKHR &swapChainSurfaceFormat,
                                    VkSampleCountFlagBits msaaSamples)
        {
            // auto &swapChainSurfaceFormat = swapchain.swapChainSurfaceFormat;
            auto &device = context->device;
            auto &descriptorSetLayout = descriptor_resource.descriptorSetLayout;

            VkShaderModule shaderModule =
                createShaderModule(readFile("shaders/deep.spv"));
            VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
                .sType = vulkan::sType<VkPipelineShaderStageCreateInfo>(),
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = shaderModule,
                .pName = "vertMain"};
            VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
                .sType = vulkan::sType<VkPipelineShaderStageCreateInfo>(),
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = shaderModule,
                .pName = "fragMain"};

            // NOLINTNEXTLINE
            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                              fragShaderStageInfo};
            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
                .sType = vulkan::sType<VkPipelineVertexInputStateCreateInfo>(),
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &bindingDescription, // NOTE: 绑定
                .vertexAttributeDescriptionCount =
                    static_cast<uint32_t>(attributeDescriptions.size()),
                .pVertexAttributeDescriptions = attributeDescriptions.data()};

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                .sType = vulkan::sType<VkPipelineInputAssemblyStateCreateInfo>(),
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE};

            VkPipelineViewportStateCreateInfo viewportState = {
                .sType = vulkan::sType<VkPipelineViewportStateCreateInfo>(),
                .viewportCount = 1,
                .scissorCount = 1};

            VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = vulkan::sType<VkPipelineRasterizationStateCreateInfo>(),
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                // .cullMode = VK_CULL_MODE_BACK_BIT,
                // .frontFace = VK_FRONT_FACE_CLOCKWISE,
                // .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            // c15: 渲染管道也要传入采样
            // MSAA仅平滑几何边缘，但没有平滑内部填充。
            // 启用样本着色，这将进一步改善画质，尽管会增加性能成本
            // 在某些情况下，质量改进可能会很明显
            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = vulkan::sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = msaaSamples,
                // NOTE: 9. 这里可以改进内部颜色质量
                .sampleShadingEnable = VK_FALSE,
            };
            VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

            VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType = vulkan::sType<VkPipelineColorBlendStateCreateInfo>(),
                .logicOpEnable = VK_FALSE,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment};

            // c11: 深度附件现在可以使用了，但是深度测试仍然需要在图形管道中启用。
            VkFormat depthFormat =
                vulkan_context::findDepthFormat(context->physicalDevice);
            VkPipelineDepthStencilStateCreateInfo depthStencil{
                .depthTestEnable = VK_TRUE,
                .depthWriteEnable = VK_TRUE,
                .depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable = VK_FALSE};

            // NOTE: 动态的意思是 cmd 的时候需要指定，确定动态类型
            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_FRONT_FACE,
                VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY};
            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = vulkan::sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = vulkan::sType<VkPipelineLayoutCreateInfo>(),
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout}; // c8: 添加描述符集

            if (::vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                         &pipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create pipeline layout!");
            }
            // Use dynamic rendering
            vulkan::structure_chain<VkGraphicsPipelineCreateInfo,
                                    VkPipelineRenderingCreateInfo>
                pipelineCreateInfoChain = {
                    {.stageCount = 2,
                     .pStages = shaderStages,
                     .pVertexInputState = &vertexInputInfo,
                     .pInputAssemblyState = &inputAssembly,
                     .pViewportState = &viewportState,
                     .pRasterizationState = &rasterizer,
                     .pMultisampleState = &multisampling, // c15: MSAA
                     .pDepthStencilState = &depthStencil, // c11: 注入管道
                     .pColorBlendState = &colorBlending,
                     .pDynamicState = &dynamicState,
                     .layout = pipelineLayout,
                     .renderPass = VK_NULL_HANDLE},
                    {.colorAttachmentCount = 1,
                     .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
                     .depthAttachmentFormat = depthFormat}}; // c11: 启用深度模板测试

            if (::vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                            &pipelineCreateInfoChain.head(), nullptr,
                                            &graphicsPipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create graphics pipeline!");
            }

            ::vkDestroyShaderModule(device, shaderModule, nullptr);
        }

      public:
        std::vector<VkDescriptorSet> allocateDescriptorSets(size_t swapChainSize)
        {
            return descriptor_resource.allocateDescriptorSets(context->device,
                                                              swapChainSize);
        }
        void setup(vulkan_context &ctx, const VkSurfaceFormatKHR &swapChainSurfaceFormat,
                   size_t maxSwapChainImages, VkSampleCountFlagBits msaaSamples)
        {
            context = &ctx;

            descriptor_resource.setup(ctx.device,
                                      maxSwapChainImages); // NOTE: 先创建
            createGraphicsPipeline(swapChainSurfaceFormat, msaaSamples);
        }
        void teardown() noexcept
        {
            auto &device = context->device;
            // Cleanup pipeline
            if (graphicsPipeline != nullptr)
            {
                ::vkDestroyPipeline(device, graphicsPipeline, nullptr);
                graphicsPipeline = VK_NULL_HANDLE;
            }
            if (pipelineLayout != nullptr)
            {
                ::vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
                pipelineLayout = VK_NULL_HANDLE;
            }

            // c8: 类似附件
            descriptor_resource.teardown(device);
        }
    };

    // c8: 替换render_object------------------------------------------------------------
    // NOLINTBEGIN
    // ==================== Transform组件 ====================
    struct Transform
    {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};

        glm::mat4 getModelMatrix() const noexcept
        {
            auto model = glm::mat4(1.0F);
            model = glm::translate(model, position);
            model =
                glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model =
                glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model =
                glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, scale);
            return model;
        }
        void update(float deltaTime)
        {
            rotation.y += 45.0f * deltaTime;
        }
    };

    // ==================== Mesh组件 ====================
    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

        void setup(vulkan_context &context)
        {
            createVertexBuffer(context);
            createIndexBuffer(context);
        }

      private:
        void createVertexBuffer(vulkan_context &context)
        {
            auto &device = context.device;
            auto &physicalDevice = context.physicalDevice;

            VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            VkBufferCreateInfo stagingBufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, stagingBuffer, stagingBufferMemory,
                         stagingBufferInfo,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void *data; // NOLINT
            ::vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            ::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
            ::vkUnmapMemory(device, stagingBufferMemory);

            VkBufferCreateInfo bufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                .usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, vertexBuffer, vertexBufferMemory,
                         bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            copyBuffer(context, stagingBuffer, vertexBuffer, bufferSize);

            ::vkDestroyBuffer(device, stagingBuffer, nullptr);
            ::vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

        void createIndexBuffer(vulkan_context &context)
        {
            auto &device = context.device;
            auto &physicalDevice = context.physicalDevice;

            VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            VkBufferCreateInfo stagingBufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, stagingBuffer, stagingBufferMemory,
                         stagingBufferInfo,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void *data;
            ::vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            ::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
            ::vkUnmapMemory(device, stagingBufferMemory);

            VkBufferCreateInfo bufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                .usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, indexBuffer, indexBufferMemory,
                         bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            copyBuffer(context, stagingBuffer, indexBuffer, bufferSize);

            ::vkDestroyBuffer(device, stagingBuffer, nullptr);
            ::vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

        static void copyBuffer(vulkan_context &context, VkBuffer srcBuffer,
                               VkBuffer dstBuffer, VkDeviceSize size)
        {
            auto &graphicsQueue = context.queue;
            auto &commandPool = context.commandPool;
            auto &device = context.device;

            VkCommandBufferAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

            VkCommandBuffer commandBuffer;
            ::vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            ::vkBeginCommandBuffer(commandBuffer, &beginInfo);

            VkBufferCopy copyRegion = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = size,
            };
            ::vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

            ::vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
            };
            ::vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            ::vkQueueWaitIdle(graphicsQueue);

            ::vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        }

      public:
        void bind(VkCommandBuffer commandBuffer) noexcept
        {
            VkDeviceSize offsets[] = {0};
            ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
            ::vkCmdBindIndexBuffer(
                commandBuffer, indexBuffer, 0,
                VK_INDEX_TYPE_UINT32); // c12: 要匹配上：uint32_t 顶点索引类型
        }

        void draw(VkCommandBuffer commandBuffer) noexcept
        {
            ::vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0,
                               0, 0);
        }

        void teardown(VkDevice device) noexcept
        {
            if (indexBuffer != VK_NULL_HANDLE)
            {
                ::vkDestroyBuffer(device, indexBuffer, nullptr);
                indexBuffer = VK_NULL_HANDLE;
            }
            if (indexBufferMemory != VK_NULL_HANDLE)
            {
                ::vkFreeMemory(device, indexBufferMemory, nullptr);
                indexBufferMemory = VK_NULL_HANDLE;
            }
            if (vertexBuffer != VK_NULL_HANDLE)
            {
                ::vkDestroyBuffer(device, vertexBuffer, nullptr);
                vertexBuffer = VK_NULL_HANDLE;
            }
            if (vertexBufferMemory != VK_NULL_HANDLE)
            {
                ::vkFreeMemory(device, vertexBufferMemory, nullptr);
                vertexBufferMemory = VK_NULL_HANDLE;
            }
        }
    };

    // ==================== Uniform Buffer组件 ====================
    struct UniformBuffer
    {
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceMemory> memories;
        std::vector<void *> mapped;

        void setup(vulkan_context &context, size_t swapChainSize)
        {
            auto &device = context.device;
            auto &physicalDevice = context.physicalDevice;

            buffers.resize(swapChainSize);
            memories.resize(swapChainSize);
            mapped.resize(swapChainSize);

            VkDeviceSize bufferSize = sizeof(UniformBufferObject);

            for (size_t i = 0; i < swapChainSize; i++)
            {
                VkBufferCreateInfo bufferInfo = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    .size = bufferSize,
                    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                };

                if (::vkCreateBuffer(device, &bufferInfo, nullptr, &buffers[i]) !=
                    VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create uniform buffer!");
                }

                VkMemoryRequirements memRequirements;
                ::vkGetBufferMemoryRequirements(device, buffers[i], &memRequirements);

                VkMemoryAllocateInfo allocInfo = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                    .allocationSize = memRequirements.size,
                    .memoryTypeIndex =
                        findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

                if (::vkAllocateMemory(device, &allocInfo, nullptr, &memories[i]) !=
                    VK_SUCCESS)
                {
                    throw std::runtime_error("failed to allocate uniform buffer memory!");
                }
                ::vkBindBufferMemory(device, buffers[i], memories[i], 0);

                ::vkMapMemory(device, memories[i], 0, bufferSize, 0, &mapped[i]);
            }
        }

        void update(size_t currentImage, const UniformBufferObject &ubo)
        {
            ::memcpy(mapped[currentImage], &ubo, sizeof(ubo));
        }

      public:
        void teardown(VkDevice device)
        {
            for (size_t i = 0; i < buffers.size(); i++)
            {
                if (mapped[i] != nullptr)
                {
                    ::vkUnmapMemory(device, memories[i]);
                    mapped[i] = nullptr;
                }
                if (buffers[i] != VK_NULL_HANDLE)
                {
                    ::vkDestroyBuffer(device, buffers[i], nullptr);
                    buffers[i] = VK_NULL_HANDLE;
                }
                if (memories[i] != VK_NULL_HANDLE)
                {
                    ::vkFreeMemory(device, memories[i], nullptr);
                    memories[i] = VK_NULL_HANDLE;
                }
            }
            buffers.clear();
            memories.clear();
            mapped.clear();
        }
    };

    // C9: 纹理对象
    struct texture_image
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = nullptr;

        vulkan_context *context = nullptr;

        void transitionImageLayout(VkDevice &device, VkQueue &graphicsQueue,
                                   VkCommandPool &commandPool, VkImage &image,
                                   VkImageLayout oldLayout, VkImageLayout newLayout,
                                   uint32_t levelCount)
        {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

            VkImageMemoryBarrier barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = levelCount,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};

            VkPipelineStageFlags sourceStage;
            VkPipelineStageFlags destinationStage;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                     newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else
            {
                throw std::invalid_argument("unsupported layout transition!");
            }

            vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                                 nullptr, 0, nullptr, 1, &barrier);

            endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
        }

        static void copyBufferToImage(VkDevice &device, VkQueue &graphicsQueue,
                                      VkCommandPool &commandPool, VkImage &image,
                                      VkBuffer buffer, uint32_t width, uint32_t height)
        {

            VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

            VkBufferImageCopy region{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .mipLevel = 0,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1},
                .imageOffset = {0, 0, 0},
                .imageExtent = {width, height, 1}};

            vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
        }

        // c14: 告诉采样器如何采样：通过绑定或提交命令
        void generateMipmaps(VkImage &image, VkFormat imageFormat, int32_t texWidth,
                             int32_t texHeight, uint32_t mipLevels)
        {
            auto &physicalDevice = context->physicalDevice;
            auto &device = context->device;
            auto &commandPool = context->commandPool;
            auto &graphicsQueue = context->queue;
            /*
            这样的内置函数生成所有mip级别非常方便，
            但遗憾的是不能保证所有平台都支持，它需要我们使用的纹理图像格式来支持线性过滤
            */
            // NOTE: 4. 要求线性滤波
            //  Check if image format supports linear blit-ing
            VkFormatProperties formatProperties =
                vulkan_context::getPhysicalDeviceFormatProperties(physicalDevice,
                                                                  imageFormat);

            if (!(formatProperties.optimalTilingFeatures &
                  VkFormatFeatureFlagBits::
                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
            {
                throw std::runtime_error(
                    "texture image format does not support linear blitting!");
            }

            VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

            VkImageMemoryBarrier barrier = {
                .sType = sType<VkImageMemoryBarrier>(),
                .srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {.aspectMask =
                                         VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};

            int32_t mipWidth = texWidth;
            int32_t mipHeight = texHeight;

            // 我们将进行几个转换，所以我们将重用这个VkImageMemoryBarrier。上面设置的字段对于所有屏障将保持不变
            for (uint32_t i = 1; i < mipLevels; i++)
            {
                // 将第i-1级从TRANSFER_DST_OPTIMAL转换为TRANSFER_SRC_OPTIMAL
                barrier.subresourceRange.baseMipLevel = i - 1;
                barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                    nullptr, 0, nullptr, 1, &barrier);

                // 设置blit操作的参数
                VkImageBlit blit = {};
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = 1;

                blit.srcOffsets[0] = {.x = 0, .y = 0, .z = 0};
                blit.srcOffsets[1] = {.x = mipWidth, .y = mipHeight, .z = 1};

                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount = 1;

                blit.dstOffsets[0] = {.x = 0, .y = 0, .z = 0};
                blit.dstOffsets[1] = {.x = mipWidth > 1 ? mipWidth / 2 : 1,
                                      .y = mipHeight > 1 ? mipHeight / 2 : 1,
                                      .z = 1};

                // NOTE: 该命令执行复制、缩放和过滤操作。
                // 我们的纹理图像现在有多个mip级别，但是暂存缓冲区只能用于填充mip级别0
                // 如果您使用专用传输队列（如顶点缓冲区中建议的），请注意：vkCmdBlitImage必须提交到具有图形功能的队列
                vkCmdBlitImage(commandBuffer, image,
                               VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                               VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &blit, VK_FILTER_LINEAR);

                // 将第i-1级从TRANSFER_SRC_OPTIMAL转换为SHADER_READ_ONLY_OPTIMAL
                barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout =
                    VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                    nullptr, 0, nullptr, 1, &barrier);

                // 我们将当前mip维度除以2。我们在除法之前检查每个维度，以确保维度永远不会变成0
                if (mipWidth > 1)
                    mipWidth /= 2;
                if (mipHeight > 1)
                    mipHeight /= 2;
            }

            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                commandBuffer, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                nullptr, 0, nullptr, 1, &barrier);

            endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);

            // NOTE: 应该注意的是，在运行时生成mipmap级别在实践中并不常见。
            //  通常它们是预先生成的，并与基本级别一起存储在纹理文件中，以提高加载速度

            // NOTE:总之，每个mip级别都要像加载原始图像一样加载到图像中。
        }
        auto createTextureImage(const std::string &path)
        {
            auto &device = context->device;
            auto &physicalDevice = context->physicalDevice;
            auto &queue = context->queue;
            auto &commandPool = context->commandPool;

            int texWidth, texHeight, texChannels;

#if 0
            // 在加载纹理前调用
            stbi_set_flip_vertically_on_load(true);
            stbi_uc *pixels = stbi_load(path.data(), &texWidth, &texHeight, &texChannels,
                                        STBI_rgb_alpha);
            // 加载完后可以恢复（可选）
            stbi_set_flip_vertically_on_load(false);
#else
            // 手动翻转（如果需要）
            stbi_uc *pixels = stbi_load(path.data(), &texWidth, &texHeight, &texChannels,
                                        STBI_rgb_alpha);
            for (int y = 0; y < texHeight / 2; ++y)
            {
                for (int x = 0; x < texWidth; ++x)
                {
                    for (int c = 0; c < 4; ++c)
                    {
                        int topIdx = (y * texWidth + x) * 4 + c;
                        int bottomIdx = ((texHeight - 1 - y) * texWidth + x) * 4 + c;
                        std::swap(pixels[topIdx], pixels[bottomIdx]);
                    }
                }
            }
#endif

            VkDeviceSize imageSize = texWidth * texHeight * 4; // rgba

            // C14: mipmap
            const auto k_mipLevels = static_cast<uint32_t>(std::floor(
                                         std::log2(std::max(texWidth, texHeight)))) +
                                     1;

            if (!pixels)
                throw std::runtime_error("failed to load texture image!");

            // 创建暂存缓冲区
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;

            VkBufferCreateInfo bufferInfo{.sType = sType<VkBufferCreateInfo>(),
                                          .size = imageSize,
                                          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

            createBuffer(physicalDevice, device, stagingBuffer, stagingBufferMemory,
                         bufferInfo,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            // 复制像素数据到缓冲区
            void *data;
            vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
            vkUnmapMemory(device, stagingBufferMemory);

            stbi_image_free(pixels);

            // 创建纹理图像
            VkImageCreateInfo imageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .extent = {.width = static_cast<uint32_t>(texWidth),
                           .height = static_cast<uint32_t>(texHeight),
                           .depth = 1},
                .mipLevels = k_mipLevels, // c14: 应用mipLevels
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                // c14: 添加VK_IMAGE_USAGE_TRANSFER_SRC_BIT 用于mipmap生成
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
            createImage(physicalDevice, device, this->image, this->imageMemory,
                        imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            // 转移图像布局并复制数据
            // c14: 应用mipLevels
            transitionImageLayout(device, queue, commandPool, this->image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, k_mipLevels);

            copyBufferToImage(device, queue, commandPool, this->image, stagingBuffer,
                              static_cast<uint32_t>(texWidth),
                              static_cast<uint32_t>(texHeight));

            // c14: 不再需要转换为 SHADER_READ_ONLY_OPTIMAL，因为generateMipmaps会处理

            // c14: 应用mipLevels
            generateMipmaps(image, VkFormat::VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight,
                            k_mipLevels);

            // 清理暂存缓冲区
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
            return k_mipLevels;
        }
        void createTextureSampler()
        {
            auto &physicalDevice = context->physicalDevice;
            auto &device = context->device;

            VkPhysicalDeviceProperties properties =
                vulkan_context::getPhysicalDeviceProperties(physicalDevice);
            VkSamplerCreateInfo samplerInfo{
                .sType = sType<VkSamplerCreateInfo>(),
                .magFilter = VkFilter::VK_FILTER_LINEAR,
                .minFilter = VkFilter::VK_FILTER_LINEAR,
                .mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipLodBias = 0.0f,
                // NOTE: 4. 各向异性器件特性启用
                .anisotropyEnable = VK_TRUE,
                .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                .compareEnable = VK_FALSE,
                .compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS,
                .minLod = 0.0F,
                .maxLod = 0.0F,
                .borderColor = {},
                .unnormalizedCoordinates = VK_FALSE};
            if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
                throw std::runtime_error("failed to create texture sampler view!");
        }

      public:
        void steup(vulkan_context &context, const std::string &path)
        {
            this->context = &context;
            const auto k_mipLevels = createTextureImage(path);
            // 创建图像视图
            VkImageViewCreateInfo viewInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = k_mipLevels,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            // c14: mipLevels 引用到 view中
            createImageView(context.device, imageView, viewInfo);
            createTextureSampler();
        }

        void teardown(VkDevice device)
        {
            if (sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(device, sampler, nullptr);
                sampler = VK_NULL_HANDLE;
            }
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
            if (image != VK_NULL_HANDLE)
            {
                vkDestroyImage(device, image, nullptr);
                image = VK_NULL_HANDLE;
            }
            if (imageMemory != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, imageMemory, nullptr);
                imageMemory = VK_NULL_HANDLE;
            }
        }

      private:
        static VkCommandBuffer beginSingleTimeCommands(VkDevice device,
                                                       VkCommandPool commandPool)
        {
            VkCommandBufferAllocateInfo allocInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1};

            VkCommandBuffer commandBuffer;
            vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            return commandBuffer;
        }
        static void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool,
                                          VkQueue queue, VkCommandBuffer commandBuffer)
        {
            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                    .commandBufferCount = 1,
                                    .pCommandBuffers = &commandBuffer};

            vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(queue);

            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        }
    };

    // ==================== Camera类 ====================
    // c16: 处理输入
    // c18: 根据上下左右的正确方向配置相机
    class Camera
    {
      public:
        Camera() // c18: 照抄测试结果来初始化
        {
            // 1. 相机位置：在(0,0,3) - 与之前测试一致
            eye = glm::vec3(0.0f, 0.0f, 3.0f);

            // 2. 相机看向原点(0,0,0)
            glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);

            // 3. 修正：使用Y轴向上 - 根据测试结果
            worldUp = glm::vec3(0.0f, 1.0f, 0.0f); // 原来是 (0, -1, 0)

            // 4. 计算看向方向
            glm::vec3 direction = glm::normalize(target - eye);

            // 5. 从方向向量自动计算yaw和pitch
            pitch = glm::degrees(asin(direction.y));
            yaw = glm::degrees(atan2(direction.z, direction.x));

            // 6. 运动参数
            movementSpeed = 2.5f;
            mouseSensitivity = 0.1f;
            fov = 45.0f; // 与测试一致

            // 7. 计算front/right/up向量
            updateCameraVectors();
        }

        glm::mat4 getViewMatrix() const // c18: 照抄测试结果来初始化
        {
            // 相机位置 + 朝向 + 上方向
            return glm::lookAt(eye, eye + front, up);
        }

        glm::mat4 getProjectionMatrix(float aspectRatio) const
        {
            // 与测试一致：45度FOV，近平面0.1，远平面100.0  // c18: 照抄测试结果来初始化
            return glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 100.0f);
        }

        // 新增：处理输入
        void processKeyboardInput(const glfw::Window &window, float deltaTime)
        {
            const auto &input = window.getInputState();
            float velocity = movementSpeed * deltaTime;

            if (input.keyW)
                eye += front * velocity;
            if (input.keyS)
                eye -= front * velocity;
            if (input.keyA)
                eye += right * velocity;
            if (input.keyD)
                eye -= right * velocity;
            if (input.keyQ)
                eye -= up * velocity;
            if (input.keyE)
                eye += up * velocity;

            updateCameraVectors();
        }

        void processMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch = true)
        {
            xoffset *= mouseSensitivity;
            yoffset *= mouseSensitivity;

            yaw += xoffset;
            pitch += yoffset;

            if (constrainPitch)
            {
                if (pitch > 89.0f)
                    pitch = 89.0f;
                if (pitch < -89.0f)
                    pitch = -89.0f;
            }

            updateCameraVectors();
        }

        void processMouseScroll(float yoffset)
        {
            fov -= yoffset;
            if (fov < 1.0f)
                fov = 1.0f;
            if (fov > 90.0f)
                fov = 90.0f;
        }

        // 计算基向量
        void updateCameraVectors()
        {
            glm::vec3 newFront;
            newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            newFront.y = sin(glm::radians(pitch));
            newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            front = glm::normalize(newFront);

            right = glm::normalize(glm::cross(front, worldUp));
            up = glm::normalize(glm::cross(right, front));
        }

      public:
        glm::vec3 eye{};
        glm::vec3 front{};
        glm::vec3 up{};
        glm::vec3 right{};
        glm::vec3 worldUp{};

        float yaw;
        float pitch;
        float movementSpeed;
        float mouseSensitivity;
        float fov;
    };

    // ==================== GameObject类 ====================
    class GameObject
    {
        // c19: 增添图元属性，转移其设置到对象中
      public:
        // 图元类型枚举
        enum class PrimitiveTopology
        {
            TRIANGLE_LIST,
            TRIANGLE_STRIP,
            TRIANGLE_FAN,
            LINE_LIST,
            LINE_STRIP,
            POINT_LIST
        };
        // 获取对应的Vulkan图元拓扑类型
        static VkPrimitiveTopology getVkPrimitiveTopology(PrimitiveTopology topology)
        {
            switch (topology)
            {
            case PrimitiveTopology::TRIANGLE_LIST:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case PrimitiveTopology::TRIANGLE_STRIP:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case PrimitiveTopology::TRIANGLE_FAN:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            case PrimitiveTopology::LINE_LIST:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case PrimitiveTopology::LINE_STRIP:
                return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case PrimitiveTopology::POINT_LIST:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            default:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            }
        }

        // 添加图元拓扑成员
        PrimitiveTopology primitiveTopology = PrimitiveTopology::TRIANGLE_LIST;

        // 设置图元拓扑的方法
        void setPrimitiveTopology(PrimitiveTopology topology)
        {
            primitiveTopology = topology;
        }

      private:
        Mesh mesh;
        Transform transform;
        UniformBuffer uniformBuffer;
        texture_image textureImage;
        std::vector<VkDescriptorSet> descriptorSets;

        vulkan_context *context = nullptr;

      public:
        int index; // c12: 未来区别开了。理论上位置信息是独有的
        void setup(vulkan_context &ctx, graphics_pipeline &pipeline, size_t swapChainSize,
                   const std::vector<Vertex> &vertices,
                   const std::vector<uint32_t> &indices, const std::string &path)
        {
            context = &ctx;

            mesh.vertices = vertices;
            mesh.indices = indices;
            mesh.setup(ctx);

            uniformBuffer.setup(ctx, swapChainSize);
            textureImage.steup(ctx, path);

            descriptorSets = pipeline.allocateDescriptorSets(swapChainSize);

            updateDescriptorSets(ctx.device);
        }

        void updateDescriptorSets(VkDevice &device)
        {

            for (size_t i = 0; i < descriptorSets.size(); i++)
            {
                VkDescriptorBufferInfo bufferInfo = {.buffer = uniformBuffer.buffers[i],
                                                     .offset = 0,
                                                     .range =
                                                         sizeof(UniformBufferObject)};

                // C9: 更新到描述及。 就是通过 imageView 和 sampler 传给sharder的
                VkDescriptorImageInfo imageInfo{
                    .sampler = textureImage.sampler,
                    .imageView = textureImage.imageView,
                    .imageLayout =
                        VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

                std::array descriptorWrites = {
                    VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                         .dstSet = descriptorSets[i],
                                         .dstBinding = 0,
                                         .dstArrayElement = 0,
                                         .descriptorCount = 1,
                                         .descriptorType =
                                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         .pBufferInfo = &bufferInfo},
                    VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                         .dstSet = descriptorSets[i],
                                         .dstBinding = 1, // index + 1
                                         .dstArrayElement = 0,
                                         .descriptorCount = 1,
                                         .descriptorType =
                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         .pImageInfo = &imageInfo}};
                ::vkUpdateDescriptorSets(device, descriptorWrites.size(),
                                         descriptorWrites.data(), 0, nullptr);
            }
        }

        // c12: 暂时屏蔽一切输入
        // c16: 增加鼠标输入，不再仅仅随着实践旋转
        void update(float deltaTime, const Camera &camera, float aspectRatio,
                    uint32_t currentImage, float modelRotationX = 0.0f,
                    float modelRotationY = 0.0f)
        {
            UniformBufferObject ubo{};
// c18: 先死顶点 + 死矩阵 确定上下左右。 着色器也要确定上下左右。才能设计相机
#if 0
            // 1. 模型矩阵
            ubo.model = glm::mat4(1.0f);

            // 2. 视图矩阵 - 使用正确的坐标系
            // 注意：在Vulkan中，相机应该看向-Z方向
            ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), // 相机位置
                                   glm::vec3(0.0f, 0.0f, 0.0f), // 看向原点
                                   glm::vec3(0.0f, 1.0f, 0.0f)  // Y轴向上
            );

            // 3. 投影矩阵 - 使用Vulkan的NDC范围 [0,1] 深度
            ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);

            // 4. 为Vulkan修正：Y轴翻转 + 深度范围修正
            ubo.proj[1][1] *= -1;
            // 可选：如果使用 [0,1] 深度范围（GLM_FORCE_DEPTH_ZERO_TO_ONE已定义）
            // 不需要额外修正，因为GLM已经处理了
#else
            // 为每个对象设置不同的位置
            // 1. 模型矩阵 - 使用传入的旋转参数
            auto input = glm::mat4(1.0f);

            // 应用传入的模型旋转参数
            ubo.model = glm::rotate(input, glm::radians(modelRotationY),
                                    glm::vec3(0.0f, 1.0f, 0.0f)); // 绕Y轴旋转
            ubo.model = glm::rotate(ubo.model, glm::radians(modelRotationX),
                                    glm::vec3(1.0f, 0.0f, 0.0f)); // 绕X轴旋转

            // 2. 使用传入的相机视图矩阵
            ubo.view = camera.getViewMatrix();

            // 3. 使用传入的相机投影矩阵
            ubo.proj = camera.getProjectionMatrix(aspectRatio);

            // 4. Vulkan需要Y轴翻转
            ubo.proj[1][1] *= -1;
#endif

            uniformBuffer.update(currentImage, ubo);
        }

        // c19: 在draw函数中使用
        void draw(VkCommandBuffer cmd, uint32_t currentImage,
                  VkPipelineLayout pipelineLayout)
        {
            ::vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      pipelineLayout, 0, 1, &descriptorSets[currentImage],
                                      0, nullptr);

            // 设置图元拓扑
            ::vkCmdSetPrimitiveTopology(cmd, getVkPrimitiveTopology(primitiveTopology));

            mesh.bind(cmd);
            mesh.draw(cmd);
        }

        void teardown()
        {
            if (context != nullptr)
            {
                textureImage.teardown(context->device);
                uniformBuffer.teardown(context->device);
                mesh.teardown(context->device);
            }
        }

        Transform &getTransform()
        {
            return transform;
        }
    };
    // NOLINTEND
    //-----------------------------------------------------------------------

    struct render
    {
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2; // NOLINT

        // NOLINTBEGIN
        // std::vector<per_frame> frames;
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkSemaphore> presentCompleteSemaphore;
        std::vector<VkSemaphore> renderFinishedSemaphore;
        std::vector<VkFence> inFlightFences;
        uint32_t semaphoreIndex = 0;
        uint32_t currentFrame = 0; // NOLINTEND

        vulkan::presentation *presentation{};  // NOLINT
        vulkan::graphics_pipeline *pipeline{}; // NOLINT

        vulkan::Camera camera; // NOLINT

      public:
        void createCommandBuffers()
        {
            auto &device = presentation->context->device;
            auto &commandPool = presentation->context->commandPool;

            commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

            VkCommandBufferAllocateInfo allocInfo = {
                .sType = vulkan::sType<VkCommandBufferAllocateInfo>(),
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())};

            if (::vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate command buffers!");
            }
        }
        void createSyncObjects()
        {
            auto &device = presentation->context->device;
            auto &swapChainImages = presentation->swapChainImages;

            presentCompleteSemaphore.resize(swapChainImages.size());
            renderFinishedSemaphore.resize(swapChainImages.size());
            VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = vulkan::sType<VkSemaphoreCreateInfo>()};

            for (size_t i = 0; i < swapChainImages.size(); i++)
            {
                if (::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                        &presentCompleteSemaphore[i]) != VK_SUCCESS ||
                    ::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                        &renderFinishedSemaphore[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create semaphores!");
                }
            }

            VkFenceCreateInfo fenceInfo = {.sType = vulkan::sType<VkFenceCreateInfo>(),
                                           .flags = VK_FENCE_CREATE_SIGNALED_BIT};
            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                if (::vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                    VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create fence!");
                }
            }
        }

        void recordCommandBuffer(std::vector<vulkan::GameObject> &objects,
                                 uint32_t imageIndex)
        {
            auto &swapChainImages = presentation->swapChainImages;
            auto &swapChainImageViews = presentation->swapChainImageViews;
            auto &swapChainExtent = presentation->swapChainExtent;
            auto &graphicsPipeline = pipeline->graphicsPipeline;
            auto &commandBuffer = commandBuffers[currentFrame];

            auto &depthImage = presentation->deepImage.image;
            auto &depthImageView = presentation->deepImage.imageView;

            auto &colorImage = presentation->colorImage.image;
            auto &colorImageView = presentation->colorImage.imageView;

            VkCommandBufferBeginInfo beginInfo = {
                .sType = vulkan::sType<VkCommandBufferBeginInfo>()};

            if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // Before starting rendering, transition the swapchain image to
            // COLOR_ATTACHMENT_OPTIMAL
            transitionImageLayout(
                commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                {}, // srcAccessMask (no need to wait for previous operations)
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

            // c15: Transition the multisampled color image to COLOR_ATTACHMENT_OPTIMAL
            transitionImageLayout(
                commandBuffer, colorImage, VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT);

            // c11: 将深度图像附加添加到布局中
            transitionImageLayout(
                commandBuffer, depthImage, VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};
            // c15: 修改颜色参数，添加颜色采样。这就是添加颜色附加了
            VkRenderingAttachmentInfo colorAttachment = {
                .sType = vulkan::sType<VkRenderingAttachmentInfo>(),
                .imageView = colorImageView,
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode = VkResolveModeFlagBits::VK_RESOLVE_MODE_AVERAGE_BIT,
                .resolveImageView = swapChainImageViews[imageIndex],
                .resolveImageLayout =
                    VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = clearColor};

            // C11: 深度清除值值
            VkClearValue clearDepth = {.depthStencil = {.depth = 1.0F, .stencil = 0}};
            VkRenderingAttachmentInfo depthAttachmentInfo = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = depthImageView,
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue = clearDepth};

            VkRenderingInfo renderingInfo = {
                .sType = vulkan::sType<VkRenderingInfo>(),
                .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment,
                .pDepthAttachment = &depthAttachmentInfo}; // C11: 渲染包含包含深度附件

            ::vkCmdBeginRendering(commandBuffer, &renderingInfo);

            ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                graphicsPipeline);

            // c12: 动态状态设置CullMode、FrontFace 是动态属性 VK_CULL_MODE_BACK_BIT
            vkCmdSetCullMode(commandBuffer, VK_CULL_MODE_NONE); // c19: 立方体
            vkCmdSetFrontFace(commandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            {
                // c8: 绘制,更新所有游戏对象
                // 计算时间增量
                static auto lastTime = std::chrono::high_resolution_clock::now();
                auto currentTime = std::chrono::high_resolution_clock::now();
                float deltaTime =
                    std::chrono::duration<float, std::chrono::seconds::period>(
                        currentTime - lastTime)
                        .count();
                lastTime = currentTime;

                // c16: 处理输入事件
                //  处理相机输入
                if ((presentation != nullptr) && (presentation->window != nullptr))
                {
                    auto &input = presentation->window->refInputState();

                    // 处理相机旋转（右键拖动）
                    if (input.rightMousePressed)
                    {
                        camera.processMouseMovement(input.dragDeltaX, input.dragDeltaY);
                    }

                    // 处理相机平移（中键拖动）
                    if (input.middleMousePressed)
                    {
                        std::cout << "middleMousePressed\n";
                        // 使用右方向向量进行水平平移，上方向向量进行垂直平移
                        float panSpeed = 0.01F;
                        camera.eye -= camera.right * input.dragDeltaX * panSpeed;
                        camera.eye += camera.up * input.dragDeltaY * panSpeed;
                        camera.updateCameraVectors();
                    }

                    // 处理相机缩放（滚轮）
                    if (input.scrollOffset != 0.0F)
                    {
                        camera.processMouseScroll(input.scrollOffset);
                    }

                    // 处理键盘输入（QWEASD）
                    camera.processKeyboardInput(*presentation->window, deltaTime);

                    // 处理模型旋转（左键拖动）
                    static float modelRotationX = 0.0F;
                    static float modelRotationY = 0.0F;
                    if (input.leftMousePressed)
                    {
                        float rotationSpeed = 0.5F;
                        modelRotationY += input.dragDeltaX * rotationSpeed;
                        modelRotationX += input.dragDeltaY * rotationSpeed;
                    }

                    // 更新所有游戏对象
                    float aspectRatio =
                        static_cast<float>(presentation->swapChainExtent.width) /
                        static_cast<float>(presentation->swapChainExtent.height);
                    for (auto &obj : objects)
                    {
                        // 传递模型旋转信息
                        obj.update(deltaTime, camera, aspectRatio, imageIndex,
                                   modelRotationX, modelRotationY);
                    }

                    // 清空增量
                    input.clearFrameDeltas();
                }

                for (auto &obj : objects)
                {
                    obj.draw(commandBuffer, imageIndex, pipeline->pipelineLayout);
                }
            }

            ::vkCmdEndRendering(commandBuffer);

            // Transition image layout for presentation
            transitionImageLayout(
                commandBuffer, swapChainImages[imageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

            if (::vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        }

        static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                          VkImageLayout oldLayout,
                                          VkImageLayout newLayout,
                                          VkAccessFlags srcAccessMask,
                                          VkAccessFlags dstAccessMask, // NOLINT
                                          VkPipelineStageFlags srcStageMask,
                                          VkPipelineStageFlags dstStageMask, // NOLINT
                                          VkImageAspectFlags aspectMask)
        {
            VkImageMemoryBarrier barrier = {
                .sType = vulkan::sType<VkImageMemoryBarrier>(),
                // Specify the pipeline stages and access masks for the barrier
                .srcAccessMask = srcAccessMask,
                .dstAccessMask = dstAccessMask,
                // Specify the old and new layouts of the image
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                // We are not changing the ownership between queues
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                // Specify the image to be affected by this barrier
                .image = image,
                // Define the subresource range (which parts of the image are affected)
                .subresourceRange = {.aspectMask = aspectMask,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                                   nullptr, 0, nullptr, 1, &barrier);
        }

        void drawFrame(std::vector<vulkan::GameObject> &objects)
        {
            auto &swapChain = presentation->swapChain;
            auto &device = presentation->context->device;
            auto &queue = presentation->context->queue;
            auto &window = presentation->window;

            ::vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                              UINT64_MAX);

            uint32_t imageIndex; // NOLINT
            VkResult result = ::vkAcquireNextImageKHR(
                device, swapChain, UINT64_MAX, presentCompleteSemaphore[semaphoreIndex],
                VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                presentation->recreateSwapChain();
                return;
            }
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            ::vkResetFences(device, 1, &inFlightFences[currentFrame]);
            ::vkResetCommandBuffer(commandBuffers[currentFrame], 0);
            recordCommandBuffer(objects, imageIndex);

            // NOLINTNEXTLINE
            VkPipelineStageFlags waitStages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

            VkSubmitInfo submitInfo = {
                .sType = vulkan::sType<VkSubmitInfo>(),
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &presentCompleteSemaphore[semaphoreIndex],
                .pWaitDstStageMask = waitStages,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffers[currentFrame],
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinishedSemaphore[imageIndex]};

            if (::vkQueueSubmit(queue, 1, &submitInfo, inFlightFences[currentFrame]) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            VkPresentInfoKHR presentInfo = {.sType = vulkan::sType<VkPresentInfoKHR>(),
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores =
                                                &renderFinishedSemaphore[imageIndex],
                                            .swapchainCount = 1,
                                            .pSwapchains = &swapChain,
                                            .pImageIndices = &imageIndex};

            result = ::vkQueuePresentKHR(queue, &presentInfo);

            if (auto &framebufferResized = window->refFramebufferResized();
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                framebufferResized)
            {
                framebufferResized = false;
                presentation->recreateSwapChain();
            }
            else if (result != VK_SUCCESS)
                throw std::runtime_error("failed to present swap chain image!");

            semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        void setup(vulkan::presentation &presentation,
                   vulkan::graphics_pipeline &pipeline)
        {
            this->presentation = &presentation;
            this->pipeline = &pipeline;
            createCommandBuffers();
            createSyncObjects();
        }

        void teardown() noexcept
        {
            auto &device = presentation->context->device;

            // Cleanup sync objects
            for (auto *semaphore : presentCompleteSemaphore)
                ::vkDestroySemaphore(device, semaphore, nullptr);
            presentCompleteSemaphore.clear();
            for (auto *semaphore : renderFinishedSemaphore)
                ::vkDestroySemaphore(device, semaphore, nullptr);
            renderFinishedSemaphore.clear();
            for (auto *fence : inFlightFences)
                ::vkDestroyFence(device, fence, nullptr);
            inFlightFences.clear();
        }
    };
}; // namespace vulkan

// NOLINTBEGIN

class HelloTriangleApplication
{
  public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

  private:
    glfw::Window window{};
    vulkan::vulkan_context context;
    vulkan::presentation presentation;
    vulkan::graphics_pipeline pipeline;
    vulkan::render render;

    void initWindow()
    {
        constexpr uint32_t WIDTH = 800;
        constexpr uint32_t HEIGHT = 600;
        window.setup(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        VkApplicationInfo appInfo = {.sType = vulkan::sType<VkApplicationInfo>(),
                                     .pApplicationName = "Hello Triangle",
                                     .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                     .pEngineName = "No Engine",
                                     .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                     // apiVersion必须是应用程序设计使用的Vulkan的最高版本
                                     .apiVersion = VK_API_VERSION_1_3};
        context.setup(appInfo, window);
        presentation.setup(context, window);
        // 获取实际的交换链图像数量
        size_t maxSwapChainImages = presentation.swapChainImages.size();
        pipeline.setup(context, presentation.swapChainSurfaceFormat, maxSwapChainImages,
                       presentation.msaaSamples);
        render.setup(presentation, pipeline);
    }

    // NOTE:2. 加载模型: 设置vertices 和 indices
    auto loadModel(const std::string &path)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // OBJ文件由位置、法线、纹理坐标和面组成。
        tinyobj::attrib_t attrib;
        // shapes：存储模型的几何结构（多个网格）
        //  materials：存储模型的表面属性（颜色、纹理等）
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
        {
            throw std::runtime_error(warn + err);
        }

        // NOTE: 4. 用于顶点消重
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        uint32_t count = 0;
        // 把文件中的所有面组合成一个模型，因此只需迭代所有形状
        for (const auto &shape : shapes)
        {
            for (const auto &index : shape.mesh.indices)
            {
                ++count;
                Vertex vertex{};
                /*
                不幸的是，attrib.vertices数组是一个float数组，而不是glm::vec3，
                因此您需要将索引乘以3。同样，每个条目有两个纹理坐标分量。
                0、1和2的偏移量用于访问X、Y和Z分量，或者在纹理坐标的情况下访问U和V分量。
                */
                vertex.pos = {attrib.vertices[3 * index.vertex_index],
                              attrib.vertices[3 * index.vertex_index + 1],
                              attrib.vertices[3 * index.vertex_index + 2]};

                // NOTE: 翻转纹理坐标的垂直分量。通过 1.0f - value 进行翻转
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index], // U 分量保持不变
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // V 分量翻转
                };

                vertex.color = {1.0f, 1.0f, 1.0f}; // 白色

                // NOTE: 每一个新顶点，才需要记录，才需要写入到vertices
                if (!uniqueVertices.contains(vertex))
                {
                    // NOTE: 需要为 vertex 添加 hash 计算能力
                    // 从1,500,000缩小到265,645！这意味着每个顶点平均在6个三角形中被重用。
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
            // NOTE: 3. 确认几何图形是否正确，确定纹理是否正确
            // 可能需要 翻转纹理坐标的垂直分量
            /// NOTE: 你会发现，背面是空的，很正常，不看到就优化。背面剔除
        }

        /*
        INFO: Total Vertex: 3828
        INFO: Loaded model: vertices=3566, indices=11484
        */
        LOGI("Total Vertex: {}", count / 3);
        LOGI("Loaded model: vertices={}, indices={}", vertices.size(), indices.size());
        LOGI("First vertex: pos=({}, {}, {}), texCoord=({}, {})", vertices[0].pos.x,
             vertices[0].pos.y, vertices[0].pos.z, vertices[0].texCoord.x,
             vertices[0].texCoord.y);

        return std::make_pair(std::move(vertices), std::move(indices));
    }

    void mainLoop()
    {
        const std::string TEXTURE_PATH = "textures/texture.jpg"; // 生产纹理
        size_t swapChainSize = presentation.swapChainImages.size();

        // ==================== 测试场景1：两个重叠的矩形 ====================
        // 创建一个大矩形（蓝色）和一个小矩形（红色），小矩形在大矩形中心重叠

        // 大矩形顶点 (蓝色)
        std::vector<Vertex> bigRectVertices = {
            // 位置 (X, Y, Z), 颜色 (R, G, B), 纹理坐标 (U, V)
            {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},  // 左上 - 蓝色
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},   // 右上 - 蓝色
            {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // 右下 - 蓝色
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 左下 - 蓝色
        };

        std::vector<uint32_t> bigRectIndices = {0, 1, 2, 2, 3, 0};

        // 小矩形顶点 (红色) - 位于大矩形中心
        std::vector<Vertex> smallRectVertices = {
            {{-0.25f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},  // 左上 - 红色
            {{0.25f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},   // 右上 - 红色
            {{0.25f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // 右下 - 红色
            {{-0.25f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 左下 - 红色
        };

        std::vector<uint32_t> smallRectIndices = {0, 1, 2, 2, 3, 0};

        // 创建游戏对象
        std::vector<vulkan::GameObject> gameObjects;

        // 先创建大矩形（蓝色）
        {
            vulkan::GameObject bigRect;
            bigRect.index = 0;
            bigRect.setup(context, pipeline, swapChainSize, bigRectVertices,
                          bigRectIndices, TEXTURE_PATH);
            gameObjects.push_back(std::move(bigRect));
        }

        // 后创建小矩形（红色） - 注意：在循环中会先绘制大矩形，后绘制小矩形
        {
            vulkan::GameObject smallRect;
            smallRect.index = 1;
            smallRect.setup(context, pipeline, swapChainSize, smallRectVertices,
                            smallRectIndices, TEXTURE_PATH);
            gameObjects.push_back(std::move(smallRect));
        }

        // 打印当前配置信息
        LOGI("=== 测试阶段1：当前配置 ===");
        LOGI("当前深度测试: 启用");
        LOGI("当前深度写入: 启用");
        LOGI("绘制顺序: 先绘制蓝色大矩形，后绘制红色小矩形");
        LOGI("预期效果: 由于深度测试和深度写入都启用，可能出现意外行为");

        // 在渲染循环中更新所有对象
        while (!window.shouldClose())
        {
            window.pollEvents();
            render.drawFrame(gameObjects);
        }

        // Don't release anything until the GPU is completely idle.
        ::vkDeviceWaitIdle(context.device);

        // 清理游戏对象
        for (auto &obj : gameObjects)
        {
            obj.teardown();
        }
        gameObjects.clear();
    }

    void cleanup() noexcept
    {

        render.teardown();

        // Cleanup pipeline
        pipeline.teardown();
        presentation.teardown();

        context.teardown();
        window.teardown();
    }
};

int main()
{
    vulkan::vulkan_context::printAvailableInstanceExtension();
    // NOTE: 相比于 hello_triangle2 实际增加的代码并不多。框架的代码也是很多的
    // NOTE: 可读性可以自己提升，没那么难
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
// NOLINTEND