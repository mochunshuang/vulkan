

export module mcs_vulkan.api;

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

import std;
import std.compat;

// ============================copy macro start=================================
#define VK_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))
#define VK_MAKE_API_VERSION(variant, major, minor, patch)            \
    ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) | \
     (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))

#define VK_API_VERSION_1_3 VK_MAKE_API_VERSION(0, 1, 3, 0)

// ============================copy macro end=================================

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

export namespace mcs::vulkan::api
{
    struct vulkan_image
    {
        vk::raii::Image image = nullptr;
        vk::raii::DeviceMemory imageMemory = nullptr;
        vk::raii::ImageView imageView = nullptr;

        void cleanup() noexcept
        {
            imageView.clear();
            imageMemory.clear();
            image.clear();
        }
    };

    struct vulkan_config
    {
        static auto &enable_layers()
        {
            static std::vector<char const *> layers = [] {
                std::vector<char const *> layers;
                if (enableValidationLayers)
                {
                    layers.push_back("VK_LAYER_KHRONOS_validation");
                }
                return layers;
            }();
            return layers;
        }
        static auto &extensions()
        {
            static std::vector<char const *> extensions;
            return extensions;
        }
    };

    struct debug_ability
    {
        vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

        static vk::Bool32 defaultDebugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
            vk::DebugUtilsMessageTypeFlagsEXT type,
            const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void * /*pUserData*/)
        {
            if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
                severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
            {
                std::cerr << "validation layer: type " << vk::to_string(type)
                          << " msg: " << pCallbackData->pMessage << '\n';
            }

            return vk::False;
        }

        constexpr void setupDebugMessenger(
            vk::raii::Instance &instance,
            decltype(defaultDebugCallback) callback = &defaultDebugCallback) noexcept
        {
            if constexpr (enableValidationLayers)
            {
                constexpr vk::DebugUtilsMessageSeverityFlagsEXT k_severity_flags(
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
                constexpr vk::DebugUtilsMessageTypeFlagsEXT k_message_type_flags(
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

                vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
                    .messageSeverity = k_severity_flags,
                    .messageType = k_message_type_flags,
                    .pfnUserCallback = callback,
                    .pUserData = nullptr};

                debugMessenger = instance.createDebugUtilsMessengerEXT(
                    debugUtilsMessengerCreateInfoEXT);
            }
        }
        void cleanup()
        {
            debugMessenger.clear();
        }
    };

    struct vulkan_instace // NOLINTBEGIN
    {
        vk::raii::Context context;
        vk::raii::Instance instance{nullptr};
        debug_ability debug;

      private:
        auto &checkLayer()
        {
            auto &requiredLayers = vulkan_config::enable_layers();

            // NOTE: 检查是否所有请求的图层都可用
            // NOTE: 匹配的是 enumerateInstanceLayerProperties 实例层属性
            // Check if the required layers are supported by the Vulkan
            // implementation.
            auto layerProperties = context.enumerateInstanceLayerProperties();
            for (auto const &requiredLayer : requiredLayers)
            {
                if (std::ranges::none_of(
                        layerProperties, [requiredLayer](auto const &layerProperty) {
                            return strcmp(layerProperty.layerName, requiredLayer) == 0;
                        }))
                {
                    throw std::runtime_error("Required layer not supported: " +
                                             std::string(requiredLayer));
                }
            }
            return requiredLayers;
        }

        auto checkExtensions()
        {

            // Get the required extensions.
            auto requiredExtensions = vulkan_config::extensions();

            // NOTE: 匹配的是 enumerateInstanceExtensionProperties 实例扩展属性
            // Check if the required extensions are supported by the Vulkan
            // implementation.
            auto extensionProperties = context.enumerateInstanceExtensionProperties();
            for (auto const &requiredExtension : requiredExtensions)
            {
                if (std::ranges::none_of(
                        extensionProperties,
                        [requiredExtension](auto const &extensionProperty) {
                            return strcmp(extensionProperty.extensionName,
                                          requiredExtension) == 0;
                        }))
                {
                    throw std::runtime_error("Required extension not supported: " +
                                             std::string(requiredExtension));
                }
            }
            return requiredExtensions;
        }

      public:
        void setup_instance()
        {
            // NOTE: 配置 我们应用程序的信息
            constexpr vk::ApplicationInfo k_app_info{
                .pApplicationName = "Hello Triangle",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "No Engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = vk::ApiVersion14};

            std::vector<char const *> requiredLayers = checkLayer();
            auto requiredExtensions = checkExtensions();

            // NOTE: layer,extensions 绑定 instance
            vk::InstanceCreateInfo createInfo{
                .pApplicationInfo = &k_app_info,
                .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
                .ppEnabledLayerNames = requiredLayers.data(),
                .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
                .ppEnabledExtensionNames = requiredExtensions.data()};
            instance = vk::raii::Instance(context, createInfo);
        }
        void setup_dabug() noexcept
        {
            debug.setupDebugMessenger(instance);
        }

        void cleanup()
        {
            debug.cleanup();
            instance.clear();
        }

    }; // NOLINTEND

    struct vulkan_device
    {
        vk::raii::PhysicalDevice physicalDevice = nullptr; // NOTE: 显卡
        vk::raii::Device device = nullptr;                 // NOTE: 逻辑设备
        vk::raii::Queue queue = nullptr;                   // NOTE: 图形队列
        uint32_t queueIndex = ~0;

        vk::raii::CommandPool commandPool = nullptr;

        vulkan_device() = default;
        vulkan_device(vulkan_device &&) = delete;
        vulkan_device(const vulkan_device &other) = delete;
        vulkan_device &operator=(const vulkan_device &) = delete;
        vulkan_device &operator=(vulkan_device &&) = delete;

      private:
        // NOTE: 显卡功能需求
        std::vector<const char *> requiredDeviceExtension = {
            vk::KHRSwapchainExtensionName, // NOTE: 交换链扩展要求
            vk::KHRSpirv14ExtensionName, vk::KHRSynchronization2ExtensionName,
            vk::KHRCreateRenderpass2ExtensionName};

        // NOTE: 选择物理设备: 选择支持我们所需功能的显卡
        void pickPhysicalDevice(vk::raii::Instance &instance)
        {
            std::vector<vk::raii::PhysicalDevice> devices =
                instance.enumeratePhysicalDevices();
            const auto devIter = std::ranges::find_if(devices, [&](auto const &device) {
                // Check if the device supports the Vulkan 1.3 API version
                bool supportsVulkan1_3 =
                    device.getProperties().apiVersion >= VK_API_VERSION_1_3;

                // Check if any of the queue families support graphics operations
                auto queueFamilies = device.getQueueFamilyProperties();
                bool supportsGraphics =
                    std::ranges::any_of(queueFamilies, [](auto const &qfp) {
                        return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
                    });

                // Check if all required device extensions are available
                auto availableDeviceExtensions =
                    device.enumerateDeviceExtensionProperties();
                bool supportsAllRequiredExtensions = std::ranges::all_of(
                    requiredDeviceExtension,
                    [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
                        return std::ranges::any_of(
                            availableDeviceExtensions,
                            [requiredDeviceExtension](
                                auto const &availableDeviceExtension) {
                                return strcmp(availableDeviceExtension.extensionName,
                                              requiredDeviceExtension) == 0;
                            });
                    });

                auto features = device.template getFeatures2<
                    vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                bool supportsRequiredFeatures =
                    features.template get<vk::PhysicalDeviceFeatures2>()
                        .features.samplerAnisotropy &&
                    features.template get<vk::PhysicalDeviceVulkan13Features>()
                        .dynamicRendering &&
                    features
                        .template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
                        .extendedDynamicState;

                return supportsVulkan1_3 && supportsGraphics &&
                       supportsAllRequiredExtensions && supportsRequiredFeatures;
            });
            if (devIter != devices.end())
            {
                physicalDevice = *devIter;
            }
            else
            {
                throw std::runtime_error("failed to find a suitable GPU!");
            }
        }

        void createLogicalDevice(vk::raii::SurfaceKHR &surface)
        {
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
                physicalDevice.getQueueFamilyProperties();

            // get the first index into queueFamilyProperties which supports both graphics
            // and present
            for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();
                 qfpIndex++)
            {
                if ((queueFamilyProperties[qfpIndex].queueFlags &
                     vk::QueueFlagBits::eGraphics) &&
                    physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
                {
                    // found a queue family that supports both graphics and present
                    queueIndex = qfpIndex;
                    break;
                }
            }
            if (queueIndex == ~0)
            {
                throw std::runtime_error(
                    "Could not find a queue for graphics and present -> terminating");
            }

            // query for Vulkan 1.3 features
            vk::StructureChain<vk::PhysicalDeviceFeatures2,
                               vk::PhysicalDeviceVulkan13Features,
                               vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
                featureChain = {
                    {.features = {.samplerAnisotropy =
                                      true}}, // vk::PhysicalDeviceFeatures2
                    {.synchronization2 = true,
                     .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
                    {.extendedDynamicState =
                         true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
                };

            // create a Device
            float queuePriority = 0.0f;
            vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
                .queueFamilyIndex = queueIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority};
            vk::DeviceCreateInfo deviceCreateInfo{
                .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &deviceQueueCreateInfo,
                .enabledExtensionCount =
                    static_cast<uint32_t>(requiredDeviceExtension.size()),
                .ppEnabledExtensionNames = requiredDeviceExtension.data()};

            device = vk::raii::Device(physicalDevice, deviceCreateInfo);
            queue = vk::raii::Queue(device, queueIndex, 0);
        }

      public:
        void setup_device(vk::raii::Instance &instance, vk::raii::SurfaceKHR &surface)
        {
            pickPhysicalDevice(instance);
            createLogicalDevice(surface);
        }

        void createCommandPool()
        {
            vk::CommandPoolCreateInfo poolInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queueIndex};
            commandPool = vk::raii::CommandPool(device, poolInfo);
        }

        vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates,
                                       vk::ImageTiling tiling,
                                       vk::FormatFeatureFlags features)
        {
            /*
            格式的支持取决于平铺模式和用法:
                linearTilingFeatures：线性平铺支持的用例
                optimalTilingFeatures：最佳平铺支持的用例
                bufferFeatures：缓冲区支持的用例

            */
            auto formatIt = std::ranges::find_if(candidates, [&](auto const format) {
                vk::FormatProperties props = physicalDevice.getFormatProperties(format);
                return (((tiling == vk::ImageTiling::eLinear) &&
                         ((props.linearTilingFeatures & features) == features)) ||
                        ((tiling == vk::ImageTiling::eOptimal) &&
                         ((props.optimalTilingFeatures & features) == features)));
            });
            if (formatIt == candidates.end())
            {
                throw std::runtime_error("failed to find supported format!");
            }
            return *formatIt;
        }

        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
        {
            vk::PhysicalDeviceMemoryProperties memProperties =
                physicalDevice.getMemoryProperties();

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

        vk::raii::ImageView createImageView(vk::raii::Image &image, vk::Format format,
                                            vk::ImageAspectFlags aspectFlags)
        {
            vk::ImageViewCreateInfo viewInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = {aspectFlags, 0, 1, 0, 1}};
            return vk::raii::ImageView(device, viewInfo);
        }
        [[nodiscard]] vk::raii::ShaderModule createShaderModule(
            const std::vector<char> &code) const
        {
            vk::ShaderModuleCreateInfo createInfo{
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t *>(code.data())};
            vk::raii::ShaderModule shaderModule{device, createInfo};

            return shaderModule;
        }
        void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                          vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer,
                          vk::raii::DeviceMemory &bufferMemory)
        {
            vk::BufferCreateInfo bufferInfo{
                .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
            buffer = vk::raii::Buffer(device, bufferInfo);
            vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
            vk::MemoryAllocateInfo allocInfo{
                .allocationSize = memRequirements.size,
                .memoryTypeIndex =
                    findMemoryType(memRequirements.memoryTypeBits, properties)};
            bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
            buffer.bindMemory(bufferMemory, 0);
        }
        void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                        vk::DeviceSize size)
        {

            vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                    .level =
                                                        vk::CommandBufferLevel::ePrimary,
                                                    .commandBufferCount = 1};
            vk::raii::CommandBuffer commandCopyBuffer =
                std::move(device.allocateCommandBuffers(allocInfo).front());
            commandCopyBuffer.begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
            commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer,
                                         vk::BufferCopy{.size = size});
            commandCopyBuffer.end();
            queue.submit(vk::SubmitInfo{.commandBufferCount = 1,
                                        .pCommandBuffers = &*commandCopyBuffer},
                         nullptr);
            queue.waitIdle();
        }

        void cleanup()
        {
            commandPool.clear();
            queue.clear();
            device.clear();
            physicalDevice.clear();
        }
    };

} // namespace mcs::vulkan::api
