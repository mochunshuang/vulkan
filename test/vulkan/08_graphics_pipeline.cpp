#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

#include <cassert>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
import std;
import std.compat;

// NOLINTBEGIN
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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
    GLFWwindow *window = nullptr;
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::SurfaceKHR surface = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;
    vk::raii::Queue queue = nullptr;
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR swapChainSurfaceFormat;
    vk::Extent2D swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    std::vector<const char *> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName, vk::KHRCreateRenderpass2ExtensionName};

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo{.pApplicationName = "Hello Triangle",
                                              .applicationVersion =
                                                  VK_MAKE_VERSION(1, 0, 0),
                                              .pEngineName = "No Engine",
                                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                              .apiVersion = vk::ApiVersion14};

        // Get the required layers
        std::vector<char const *> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
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

        // Get the required extensions.
        auto requiredExtensions = getRequiredExtensions();

        // Check if the required extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (auto const &requiredExtension : requiredExtensions)
        {
            if (std::ranges::none_of(extensionProperties,
                                     [requiredExtension](auto const &extensionProperty) {
                                         return strcmp(extensionProperty.extensionName,
                                                       requiredExtension) == 0;
                                     }))
            {
                throw std::runtime_error("Required extension not supported: " +
                                         std::string(requiredExtension));
            }
        }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data()};
        instance = vk::raii::Instance(context, createInfo);
    }

    void setupDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType = messageTypeFlags,
            .pfnUserCallback = &debugCallback};
        debugMessenger =
            instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    void createSurface()
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    void pickPhysicalDevice()
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
            auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions = std::ranges::all_of(
                requiredDeviceExtension,
                [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
                    return std::ranges::any_of(
                        availableDeviceExtensions,
                        [requiredDeviceExtension](auto const &availableDeviceExtension) {
                            return strcmp(availableDeviceExtension.extensionName,
                                          requiredDeviceExtension) == 0;
                        });
                });

            auto features = device.template getFeatures2<
                vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures =
                features.template get<vk::PhysicalDeviceVulkan13Features>()
                    .dynamicRendering &&
                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
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

    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
            physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports both graphics and
        // present
        uint32_t queueIndex = ~0;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
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
                {},                         // vk::PhysicalDeviceFeatures2
                {.dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
                {.extendedDynamicState =
                     true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
            };

        // create a Device
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex,
                                                        .queueCount = 1,
                                                        .pQueuePriorities =
                                                            &queuePriority};
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

    void createSwapChain()
    {
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent = chooseSwapExtent(surfaceCapabilities);
        swapChainSurfaceFormat =
            chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));
        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = *surface,
            .minImageCount = chooseSwapMinImageCount(surfaceCapabilities),
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode =
                chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(*surface)),
            .clipped = true};

        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    void createImageViews()
    {
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainSurfaceFormat.format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        for (auto image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    // NOTE: 创建管线
    void createGraphicsPipeline() {}

    static uint32_t chooseSwapMinImageCount(
        vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
    {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) &&
            (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR> &availableFormats)
    {
        assert(!availableFormats.empty());
        const auto formatIt =
            std::ranges::find_if(availableFormats, [](const auto &format) {
                return format.format == vk::Format::eB8G8R8A8Srgb &&
                       format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            });
        return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
    }

    static vk::PresentModeKHR chooseSwapPresentMode(
        const std::vector<vk::PresentModeKHR> &availablePresentModes)
    {
        assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {
            return presentMode == vk::PresentModeKHR::eFifo;
        }));
        return std::ranges::any_of(availablePresentModes,
                                   [](const vk::PresentModeKHR value) {
                                       return vk::PresentModeKHR::eMailbox == value;
                                   })
                   ? vk::PresentModeKHR::eMailbox
                   : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
    {
        if (capabilities.currentExtent.width != 0xFFFFFFFF)
        {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width),
                std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height)};
    }

    std::vector<const char *> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL
    debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                  vk::DebugUtilsMessageTypeFlagsEXT type,
                  const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
    {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::cerr << "validation layer: type " << to_string(type)
                      << " msg: " << pCallbackData->pMessage << std::endl;
        }

        return vk::False;
    }
};
/*
(Vertex/Input Buffer)
    |
    v
🟢 [Input Assembly]
    ↙ 输入汇编器: 从指定的缓冲区收集原始顶点数据,可以使用索引缓冲区避免赋值
    |
    v
🟠 [Vertex Shader]
    ↙ 顶点着色器：针对每个顶点运行变换，比如从模型空间转换为屏幕空间
    |
    |
    v
🟠 [Tessellation]
    ↙  细分着色器：这里可以提高网格体质量。通常用于制作像
    |   砖墙和楼梯在附近时看起来不那么平坦
    |
    v
🟠 [Geometry Shader]
    ↙ 几何着色器：每个基元（三角形、线、点）上运行，并且可以丢弃它或输出比进来的更多的基元
    |
    |
    v
🟢 [Rasterization]
    ↙ 光栅化阶段：将基元分解为片段。落在屏幕外的片段都被丢弃。
    |  顶点着色器输出的属性被插值到片段上。
    |  深度测试在这里执行，还会将很多原始片段丢弃。
    v
🟠 [Fragment Shader]
    ↙ 片段着色器：幸存的片段调用，并确定片段被写入哪个帧缓冲区以及颜色和深度值。
    |  可以使用来自顶点着色器的插值数据来执行此操作，
    |  其中可以包括纹理坐标和用于照明的法线。
    |
    v
🟢 [Color blending]
    ↙ 颜色混合阶段：混合映射到帧缓冲区中同一像素的不同片段
    |  片段可以简单地相互覆盖、相加或基于透明度混合。
    |
    v
(Frame Buffer)
    ↙ 最终像素输出

一些可编程阶段是可选的。例如，如果您只是绘制简单的几何图形，可以禁用细分和几何阶段。
如果您只对深度值感兴趣，那么您可以禁用片段着色器阶段，这对阴影地图生成很有用。
*/
// 🟢阶段称为固定功能阶段。这些阶段允许您使用参数调整其操作，但它们的工作方式是预定义的。
// 🟠是可编程的，这意味着您可以将自己的代码上传到图形卡以精确应用您想要的操作
// 三角形所需的两个可编程阶段：顶点着色器和片段着色器。混合模式、视口、光栅化等固定功能配置.
int main()
{
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