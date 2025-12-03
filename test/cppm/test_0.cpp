

#include <cstddef>
#include <exception>
#include <iostream>
#include <type_traits>

#include "glfw.hpp"
#include "log.hpp"
#include "structure_chain.hpp"
#include "vulkan_utils.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <cassert>

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_profiles.hpp>
#include <print>
#include <fstream>
#include <string>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static_assert(sizeof(float) == 4);

    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {
            VkVertexInputAttributeDescription{.location = 0,
                                              .binding = 0,
                                              // .format = VkFormat::eR32G32Sfloat,
                                              .format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
                                              .offset = offsetof(Vertex, pos)},
            VkVertexInputAttributeDescription{.location = 1,
                                              .binding = 0,
                                              //   .format = VkFormat::eR32G32B32Sfloat,
                                              .format =
                                                  VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
                                              .offset = offsetof(Vertex, color)}};
    }
};

namespace vulkan
{

    struct vulkan_context
    {
        using window_type = glfw::Window;
        // NOLINTNEXTLINE
        static constexpr auto VK_DEBUG_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

        // NOLINTBEGIN
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueIndex = ~0;
        VkQueue queue = VK_NULL_HANDLE;

        VkCommandPool commandPool = VK_NULL_HANDLE;

        VkSampleCountFlagBits msaaSamples{};
        struct AppInfo // Application info to store profile support
        {
            bool profileSupported = false;
            VpProfileProperties profile;
        } appInfo = {};

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

        static void addSurfaceExtension(
            std::vector<const char *> &required_instance_extensions)
        {
            required_instance_extensions.append_range(
                window_type::getRequiredSurfaceExtensions());
        }

        static void addDebugExtension(
            std::vector<const char *> &required_instance_extensions)
        {
            required_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
        [[nodiscard]] static bool addDebuglayerIfAvailable(
            std::vector<const char *> &requested_instance_layers,
            const std::vector<VkLayerProperties> &supported_instance_layers =
                availableInstanceLayer())
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
        void destroyDebugMessenger() noexcept
        {
            if ((debug_messenger != nullptr) && (instance != nullptr))
            { // NOLINTNEXTLINE
                auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    ::vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (func != nullptr)
                {
                    func(instance, debug_messenger, nullptr);
                }
                debug_messenger = VK_NULL_HANDLE;
            }
        }

        // ---------------------cpp like----------------------------------
        [[nodiscard]] auto enumeratePhysicalDevices() const
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
        static auto getPhysicalDeviceProperties(const VkPhysicalDevice &gpu)
        {
            VkPhysicalDeviceProperties deviceProperties;
            ::vkGetPhysicalDeviceProperties(gpu, &deviceProperties);
            return deviceProperties;
        }
        // NOTE: 2. 检查是否支持，我们要求的特性
        void checkFeatureSupport()
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
                appInfo.profileSupported = true; // TODO(mcs): 如果不支持应该回退
                appInfo.profile = profileProperties;
                LOGI("Device supports Vulkan profile: {}", profileProperties.profileName);
            }
            else
            {
                LOGW("Device does not support Vulkan profile: {}",
                     profileProperties.profileName);
            }
        }
        [[nodiscard]] auto getPhysicalDeviceSurfaceSupportKHR(
            uint32_t queueFamilyIndex, VkSurfaceKHR surface) const noexcept
        {
            VkBool32 presentSupport; // NOLINT
            ::vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex,
                                                   surface, &presentSupport);
            return presentSupport;
        }

        //---------------------------------
        void createInstance(VkApplicationInfo &appInfo)
        {
            // c1: 1. extensions
            auto available_instance_extensions = availableInstanceExtension();
            std::vector<const char *> required_instance_extensions{};
            addSurfaceExtension(required_instance_extensions);
            addDebugExtension(required_instance_extensions);
            if (!checkExtensionSupport(required_instance_extensions,
                                       available_instance_extensions))
                throw std::runtime_error("Required instance extensions are missing.");

            // c1: 2. layers
            auto supported_instance_layers = availableInstanceLayer();
            std::vector<const char *> requested_instance_layers{};
            bool has_debug_utils = addDebuglayerIfAvailable(requested_instance_layers,
                                                            supported_instance_layers);
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
            std::vector<const char *> validationLayers = {};
            if (enableValidationLayers && has_debug_utils)
            {
                validationLayers.emplace_back(VK_DEBUG_LAYER_NAME);
                applyDebugCreateInfo(debugCreateInfo);
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
            window.createVkSurfaceKHR(instance, surface);
        }

        [[nodiscard]] VkSampleCountFlagBits getMaxUsableSampleCount() const noexcept
        {
            VkPhysicalDeviceProperties physicalDeviceProperties =
                getPhysicalDeviceProperties(physicalDevice);
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
        void pickPhysicalDevice(const std::vector<const char *> &requiredDeviceExtension)
        {
            std::vector<VkPhysicalDevice> gpus = enumeratePhysicalDevices();

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
                msaaSamples = getMaxUsableSampleCount();
                VkPhysicalDeviceProperties deviceProperties =
                    getPhysicalDeviceProperties(physicalDevice);
                LOGI("Selected GPU: {}. API Version: {}.{}.{}",
                     deviceProperties.deviceName,
                     VK_VERSION_MAJOR(deviceProperties.apiVersion),
                     VK_VERSION_MINOR(deviceProperties.apiVersion),
                     VK_VERSION_PATCH(deviceProperties.apiVersion));

                checkFeatureSupport(); // c2: profies check
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
                    if (getPhysicalDeviceSurfaceSupportKHR(i, surface) != 0U)
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
                    {},
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
                // .pEnabledFeatures = "";
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

      public:
        void setup(VkApplicationInfo &appInfo, window_type &window)
        {
            createInstance(appInfo);
            createSurface(window);

            /*
Querying Properties, Extensions, Features, Limits, and Formats
NOTE: Properties 只读数据的抽象
NOTE: Extensions：扩展可以添加新功能
NOTE: Features：特性描述了并非所有实现都支持的功能
Limits：限制是应用程序可能需要注意的implementation-dependent最小值、最大值和其他设备特性
NOTE: Formats: 布局
*/
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
            if ((surface != nullptr) && (instance != nullptr))
            {
                ::vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = VK_NULL_HANDLE;
            }

            // Cleanup device
            if (device != nullptr)
            {
                ::vkDestroyDevice(device, nullptr);
                device = VK_NULL_HANDLE;
            }

            // Cleanup debug messenger
            destroyDebugMessenger();

            // Cleanup instance
            if (instance != nullptr)
            {
                ::vkDestroyInstance(instance, nullptr);
                instance = VK_NULL_HANDLE;
            }
        }
    };

    struct presentation
    {
        // NOLINTBEGIN
        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        std::vector<VkImage> swapChainImages;
        VkSurfaceFormatKHR swapChainSurfaceFormat{};
        VkExtent2D swapChainExtent{};
        std::vector<VkImageView> swapChainImageViews; // NOLINTEND

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
            auto &surface = context->surface;
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

      public:
        void setup(vulkan_context &ctx, window_type &w)
        {
            context = &ctx;
            window = &w;
            createSwapChain();
            createImageViews();
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
        }
    };

    struct graphics_pipeline
    {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE; // NOLINT
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;     // NOLINT

        vulkan_context *context{}; // NOLINT

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

      public:
        void createGraphicsPipeline(const VkSurfaceFormatKHR &swapChainSurfaceFormat)
        {
            // auto &swapChainSurfaceFormat = swapchain.swapChainSurfaceFormat;
            auto &device = context->device;

            VkShaderModule shaderModule =
                createShaderModule(readFile("shaders/18_shader_vertexbuffer.spv"));
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
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = vulkan::sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
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

            // NOTE: 动态的意思是 cmd 的时候需要指定，确定动态类型
            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY};
            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = vulkan::sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = vulkan::sType<VkPipelineLayoutCreateInfo>()};

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
                     .pMultisampleState = &multisampling,
                     .pColorBlendState = &colorBlending,
                     .pDynamicState = &dynamicState,
                     .layout = pipelineLayout,
                     .renderPass = VK_NULL_HANDLE},
                    {.colorAttachmentCount = 1,
                     .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

            if (::vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                            &pipelineCreateInfoChain.head(), nullptr,
                                            &graphicsPipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create graphics pipeline!");
            }

            ::vkDestroyShaderModule(device, shaderModule, nullptr);
        }

        void setup(vulkan_context &ctx, const VkSurfaceFormatKHR &swapChainSurfaceFormat)
        {
            context = &ctx;
            createGraphicsPipeline(swapChainSurfaceFormat);
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
        }
    };

    struct render_object // NOLINTBEGIN
    {
        // 缓冲区应该可用于渲染命令，直到程序结束，并且它不依赖于交换链
        VkBuffer vertexBuffer = nullptr; // NOTE: 顶点缓冲区句柄
        VkDeviceMemory vertexBufferMemory = nullptr;

        VkBuffer indexBuffer = nullptr;
        VkDeviceMemory indexBufferMemory = nullptr;

        // c7: 两个三角形： 6个只需要分配两个
        std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
        const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

      private:
        static VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties(
            VkPhysicalDevice physicalDevice) noexcept
        {
            VkPhysicalDeviceMemoryProperties properties;
            ::vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
            return properties;
        }
        static auto getBufferMemoryRequirements(VkDevice device, VkBuffer &buffer)
        {
            VkMemoryRequirements memRequirements;
            ::vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
            return memRequirements;
        }

        // 需要添加的辅助函数
        void copyBuffer(vulkan_context &context, VkBuffer srcBuffer, VkBuffer dstBuffer,
                        VkDeviceSize size)
        {
            auto &graphicsQueue = context.queue;
            auto &commandPool = context.commandPool;
            auto &device = context.device;

            // 创建一次性命令缓冲区
            VkCommandBufferAllocateInfo allocInfo = {
                .sType = sType<VkCommandBufferAllocateInfo>(),
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

            VkCommandBuffer commandBuffer;
            ::vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

            // 开始录制命令缓冲区
            VkCommandBufferBeginInfo beginInfo = {
                .sType = sType<VkCommandBufferBeginInfo>(),
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            ::vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // 复制命令
            VkBufferCopy copyRegion = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = size,
            };
            ::vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

            // 结束录制
            ::vkEndCommandBuffer(commandBuffer);

            // 提交到队列
            VkSubmitInfo submitInfo = {
                .sType = sType<VkSubmitInfo>(),
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
            };
            ::vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            ::vkQueueWaitIdle(graphicsQueue); // 等待复制完成

            // 清理命令缓冲区
            ::vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        }

        void createBuffer(VkPhysicalDevice &physicalDevice, VkDevice &device,
                          VkBuffer &buffer, VkDeviceMemory &bufferMemory,
                          VkBufferCreateInfo &createInfo,
                          VkMemoryPropertyFlags properties)
        {
            if (::vkCreateBuffer(device, &createInfo, nullptr, &buffer) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create staging buffer!");
            }
            VkMemoryRequirements stagingMemReqs =
                getBufferMemoryRequirements(device, buffer);

            VkMemoryAllocateInfo stagingAllocInfo = {
                .sType = sType<VkMemoryAllocateInfo>(),
                .allocationSize = stagingMemReqs.size,
                .memoryTypeIndex =
                    findMemoryType(physicalDevice, stagingMemReqs.memoryTypeBits,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
            if (::vkAllocateMemory(device, &stagingAllocInfo, nullptr, &bufferMemory) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to allocate staging buffer memory!");
            if (::vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS)
                throw std::runtime_error("failed to bind buffer memory!");
        }

        // NOTE: 创建顶点缓冲区
        void createVertexBuffer(vulkan_context &context)
        {
            auto &device = context.device;
            VkPhysicalDevice &physicalDevice = context.physicalDevice;
            VkQueue &graphicsQueue = context.queue;
            VkCommandPool &commandPool = context.commandPool;

            VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

            // 1. 创建临时staging buffer（CPU可见）
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            VkBufferCreateInfo stagingBufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // C6: 用作传输源
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, stagingBuffer, stagingBufferMemory,
                         stagingBufferInfo,
                         // C6: 主机可访问且与主机相干
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            // 2. 使用vertices填充staging buffer
            void *data;
            ::vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            ::memcpy(data, vertices.data(), (size_t)bufferSize);
            ::vkUnmapMemory(device, stagingBufferMemory);

            // 3. 创建最终的顶点buffer（GPU本地）
            VkBufferCreateInfo bufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                // C6: 用作传输目标,同时用作顶点buffer
                .usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, vertexBuffer, vertexBufferMemory,
                         bufferInfo,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // C6: 设备本地内存

            // 4. 复制缓冲区（需要命令缓冲区）
            copyBuffer(context, stagingBuffer, vertexBuffer, bufferSize);

            // 5. 清理staging buffer
            ::vkDestroyBuffer(device, stagingBuffer, nullptr);
            ::vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

        // NOTE: 3. 找到合适的内存类型
        static uint32_t findMemoryType(VkPhysicalDevice &physicalDevice,
                                       uint32_t typeFilter,
                                       VkMemoryPropertyFlags properties)
        {

            VkPhysicalDeviceMemoryProperties memProperties =
                getPhysicalDeviceMemoryProperties(physicalDevice);

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
            {
                if ((typeFilter & (1 << i)) &&
                    (memProperties.memoryTypes[i].propertyFlags & properties) ==
                        properties)
                {
                    return i;
                }
            }

            throw std::runtime_error("failed to find suitable memory type!");
        }

        // c7: 创建顶点缓冲区
        void createIndexBuffer(vulkan_context &context)
        {
            auto &device = context.device;
            VkPhysicalDevice &physicalDevice = context.physicalDevice;

            VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

            // 1. 创建临时staging buffer（CPU可见）
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            VkBufferCreateInfo stagingBufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // 用作传输源
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, stagingBuffer, stagingBufferMemory,
                         stagingBufferInfo,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            // 2. 填充staging buffer
            void *data;
            ::vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            ::memcpy(data, indices.data(), (size_t)bufferSize);
            ::vkUnmapMemory(device, stagingBufferMemory);

            // 3. 创建最终的索引buffer（GPU本地）
            VkBufferCreateInfo bufferInfo = {
                .sType = sType<VkBufferCreateInfo>(),
                .size = bufferSize,
                .usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            createBuffer(physicalDevice, device, indexBuffer, indexBufferMemory,
                         bufferInfo,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // 设备本地内存

            // 4. 复制缓冲区
            copyBuffer(context, stagingBuffer, indexBuffer, bufferSize);

            // 5. 清理staging buffer
            ::vkDestroyBuffer(device, stagingBuffer, nullptr);
            ::vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

      public:
        void setup(vulkan_context &context)
        {
            createVertexBuffer(context);
            createIndexBuffer(context);
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
        // NOTE: 绑定顶点缓冲区和索引缓冲区的绘制命令
        void bindAndDraw(VkCommandBuffer commandBuffer) noexcept
        {
            // 1. 绑定顶点缓冲区
            VkDeviceSize vertexOffsets[] = {0};
            ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, vertexOffsets);
            // 2. 绑定索引缓冲区
            ::vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            // 3. 绘制命令（使用索引）
            ::vkCmdDrawIndexed(commandBuffer,
                               static_cast<uint32_t>(indices.size()), // 索引数量
                               1,                                     // 实例数量
                               0,                                     // 第一个索引
                               0,                                     // 顶点偏移
                               0);                                    // 第一个实例
        }

    }; // NOLINTEND

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

        void recordCommandBuffer(vulkan::render_object &object, uint32_t imageIndex)
        {
            auto &swapChainImages = presentation->swapChainImages;
            auto &swapChainImageViews = presentation->swapChainImageViews;
            auto &swapChainExtent = presentation->swapChainExtent;
            auto &graphicsPipeline = pipeline->graphicsPipeline;
            auto &commandBuffer = commandBuffers[currentFrame];

            VkCommandBufferBeginInfo beginInfo = {
                .sType = vulkan::sType<VkCommandBufferBeginInfo>()};

            if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // Transition image layout for rendering
            transitionImageLayout(
                commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};
            VkRenderingAttachmentInfo colorAttachment = {
                .sType = vulkan::sType<VkRenderingAttachmentInfo>(),
                .imageView = swapChainImageViews[imageIndex],
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = clearColor};

            VkRenderingInfo renderingInfo = {
                .sType = vulkan::sType<VkRenderingInfo>(),
                .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment};

            ::vkCmdBeginRendering(commandBuffer, &renderingInfo);

            ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                graphicsPipeline);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Since we declared VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY as dynamic,
            // we need to set the primitive topology here.
            // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST tells Vulkan that the input vertex data
            // should be interpreted as a list of triangles.
            ::vkCmdSetPrimitiveTopology(commandBuffer,
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            object.bindAndDraw(commandBuffer);
            ::vkCmdEndRendering(commandBuffer);

            // Transition image layout for presentation
            transitionImageLayout(commandBuffer, swapChainImages[imageIndex],
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

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
                                          VkPipelineStageFlags dstStageMask)
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
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                                   nullptr, 0, nullptr, 1, &barrier);
        }

        void drawFrame(vulkan::render_object &object)
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
            recordCommandBuffer(object, imageIndex);

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

    vulkan::render_object object;

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
        pipeline.setup(context, presentation.swapChainSurfaceFormat);
        render.setup(presentation, pipeline);

        object.setup(context);
    }

    void mainLoop()
    {
        while (!window.shouldClose())
        {
            window.pollEvents();
            render.drawFrame(object);
        }
        // Don't release anything until the GPU is completely idle.
        ::vkDeviceWaitIdle(context.device);
    }

    void cleanup() noexcept
    {

        object.teardown(context.device);

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