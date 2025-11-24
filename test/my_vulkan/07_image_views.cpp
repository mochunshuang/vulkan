
// NOTE: 没有这一行宏，createInstance 失败
#include <cassert>
#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
import std;
import std.compat;

// NOLINTBEGIN

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// NOTE: 层的概念：验证层的名字，通过名字，开启服务
const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

// NOTE: DEBUG 模式才开启验证层。是符合C++标准的
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

auto glfw_extensions()
{
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    return extensions;
}

// NOTE: 获取实例扩展信息的 公共函数
std::vector<const char *> getRequiredExtensions()
{
    std::vector extensions = glfw_extensions();
    if (enableValidationLayers)
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    return extensions;
}

auto get_extensions(vk::raii::Context &context)
{

    // Get the required extensions.
    auto requiredExtensions = getRequiredExtensions();

    // NOTE: 匹配的是 enumerateInstanceExtensionProperties 实例扩展属性
    // Check if the required extensions are supported by the Vulkan implementation.
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    for (auto const &requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(extensionProperties, [requiredExtension](
                                                          auto const &extensionProperty) {
                return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
            }))
        {
            throw std::runtime_error("Required extension not supported: " +
                                     std::string(requiredExtension));
        }
    }
    return requiredExtensions;
}

auto get_layer(vk::raii::Context &context)
{
    //  Get the required layers
    std::vector<char const *> requiredLayers;

    if (enableValidationLayers)
    {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // NOTE: 检查是否所有请求的图层都可用
    // NOTE: 匹配的是 enumerateInstanceLayerProperties 实例层属性
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
    return requiredLayers;
}

struct glfw_instance
{
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    glfw_instance() = default;
    glfw_instance(vk::raii::Context &context)
    {
        // NOTE: 配置 我们应用程序的信息
        constexpr vk::ApplicationInfo appInfo{.pApplicationName = "Hello Triangle",
                                              .applicationVersion =
                                                  VK_MAKE_VERSION(1, 0, 0),
                                              .pEngineName = "No Engine",
                                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                              .apiVersion = vk::ApiVersion14};

        std::vector<char const *> requiredLayers = get_layer(context);
        auto requiredExtensions = get_extensions(context);

        // NOTE: layer,extensions 绑定 instance
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
        if constexpr (enableValidationLayers)
        {
            constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
            constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

            // NOTE: severityFlags 和 messageTypeFlags 会传递到 debugCallback 的参数中
            vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
                .messageSeverity = severityFlags,
                .messageType = messageTypeFlags,
                .pfnUserCallback = &debugCallback,
                .pUserData = {}}; // NOTE: 不传递数据
            debugMessenger =
                instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
        }
    }

    // NOTE: 验证层消息回调函数
    // 验证层默认会将调试消息打印到标准输出中，但我们也可以通过提供显式 callback
    // 的 callback 中。
    // 这也将允许您决定哪种 您希望看到的消息，因为并非所有消息都一定是（致命的） 错误。
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void * /*pUserData*/)
    {
        /*
        第一个参数指定消息的严重性，该参数是 以下标志：
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT：诊断消息
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT：信息性消息 就像创建资源一样
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT：关于行为的信息
            这不一定是错误，但很可能是应用程序中的错误
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT：关于行为的信息
        这是无效的，可能会导致崩溃
        */
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::cerr << "validation layer: type " << to_string(type)
                      << " msg: " << pCallbackData->pMessage << std::endl;
        }
        // if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        // {
        //     // Message is important enough to show
        // }

        /*
        该参数可以具有以下值：type
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT：发生了一些与规格或性能无关的事件
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT：发生了违反规范或表明可能存在错误的事情
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT：Vulkan 的潜在非最佳用途
        */

        /*
        其中最重要的成员是：pCallbackData VkDebugUtilsMessengerCallbackDataEXT
        pMessage：调试消息为以 null 结尾的字符串
        pObjects：与消息相关的 Vulkan 对象句柄数组
        objectCount：数组中的对象数
        */

        return vk::False;
    }
};

struct logical_device
{
    vk::raii::Device device = nullptr;       // NOTE: 逻辑设备
    vk::raii::Queue graphicsQueue = nullptr; // NOTE: 图形队列

    logical_device() = default;

    logical_device(vk::raii::PhysicalDevice &physicalDevice,
                   std::vector<const char *> &requiredDeviceExtension)
    {
        createLogicalDevice(physicalDevice, requiredDeviceExtension);
    }

    void createLogicalDevice(vk::raii::PhysicalDevice &physicalDevice,
                             std::vector<const char *> &requiredDeviceExtension)
    {
        // NOTE: find the index of the first queue family that supports graphics
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
            physicalDevice.getQueueFamilyProperties();

        // NOTE: get the first index into queueFamilyProperties which supports graphics
        auto graphicsQueueFamilyProperty =
            std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {
                return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                       static_cast<vk::QueueFlags>(0); // NOTE: 要求具备图形队列族
            });
        assert(graphicsQueueFamilyProperty != queueFamilyProperties.end() &&
               "No graphics queue family found!");

        auto graphicsIndex = static_cast<uint32_t>(
            std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

        /*
        以下是此代码中发生的情况：
        我们创建了具有三种不同特征结构的vk::StructureChain
        对于链中的每个结构，我们提供了一个初始值设定项：
            第一个结构 （） 留空，并vk::PhysicalDeviceFeatures2{}
            在第二个结构中，我们启用了 Vulkan 1.3 的功能dynamicRendering
            在第三种结构中，我们从扩展启用该功能extendedDynamicState

        模板通过在它们之间设置指针来自动将这些结构连接在一起。
        这样我们就不必手动将每个结构链接到下一个结构。vk::StructureChain->pNext
        */
        // NOTE: 启用其他设备功能.为了启用多组功能，Vulkan 使用了一个称为“结构链”的概念。
        // NOTE: query for Vulkan 1.3 features
        using features0 = vk::PhysicalDeviceFeatures2;
        using features1 = vk::PhysicalDeviceVulkan13Features;
        using features2 = vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT;
        vk::StructureChain<features0, features1, features2> featureChain = {
            {},                         // vk::PhysicalDeviceFeatures2
            {.dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
            {.extendedDynamicState =
                 true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        };

        // NOTE: create a Device
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = graphicsIndex,
                                                        .queueCount = 1,
                                                        .pQueuePriorities =
                                                            &queuePriority};
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<features0>(), // NOTE: 传递第一个即可
            .queueCreateInfoCount = 1,               // NOTE: 目前仅仅需要绑定1个队列
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount =
                static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()};

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        // NOTE: 这个 device 的graphicsIndex 图形队列的句柄
        graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
    }
};

struct physical_device
{
    vk::raii::PhysicalDevice physicalDevice = nullptr; // NOTE: 显卡

    // NOTE: 显卡功能需求
    std::vector<const char *> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName, // NOTE: 交换链扩展要求
        vk::KHRSpirv14ExtensionName, vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName};

    physical_device() = default;
    physical_device(vk::raii::Instance &instance)
    {
        pickPhysicalDevice(instance);
    }
    // NOTE: 选择物理设备: 选择支持我们所需功能的显卡
    void pickPhysicalDevice(vk::raii::Instance &instance)
    {
        // NOTE: 列出显卡 通过查询 enumeratePhysicalDevices
        // std::vector 即可 目标类型可以自动实例
        std::vector devices = instance.enumeratePhysicalDevices();

        // NOTE: 枚举所有显卡. 找到满足所有显卡功能需求的显卡
        const auto devIter = std::ranges::find_if(devices, [&](auto const &device) {
            // NOTE: Check if the device supports the Vulkan 1.3 API version
            if (not(device.getProperties().apiVersion >= VK_API_VERSION_1_3))
                return false;

            // NOTE: Check if any of the queue families support graphics operations
            auto queueFamilies = device.getQueueFamilyProperties();
            bool supportsGraphics =
                std::ranges::any_of(queueFamilies, [](auto const &qfp) {
                    return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
                });
            if (not supportsGraphics) // NOTE: 要求设备队列存在图形队列支持
                return false;

            // NOTE: 要求 requiredDeviceExtension 集合的特性全部都是支持的
            // NOTE: Check if all required device extensions are available
            auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions = std::ranges::all_of(
                requiredDeviceExtension, [&](auto const &requiredDeviceExtension) {
                    return std::ranges::any_of(
                        availableDeviceExtensions,
                        [&](auto const &availableDeviceExtension) {
                            return strcmp(availableDeviceExtension.extensionName,
                                          requiredDeviceExtension) == 0;
                        });
                });
            if (not supportsAllRequiredExtensions)
                return false;

            // NOTE: 模板生成特性对象
            auto features = device.template getFeatures2<
                vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

            // NOTE: 要求支持动态渲染 dynamicRendering
            bool supportsRequiredFeatures =
                features.template get<vk::PhysicalDeviceVulkan13Features>()
                    .dynamicRendering &&
                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
                    .extendedDynamicState;

            return supportsRequiredFeatures;
        });

        if (devIter != devices.end())
            physicalDevice = *devIter;
        else
            throw std::runtime_error("failed to find a suitable GPU!");
    }
};

// NOTE: 窗户系统: 目的是让vulkan绘制的图形，可以绘制到电脑屏幕上
struct glfw_surface
{
    vk::raii::SurfaceKHR surface = nullptr; // NOTE: 窗口表面句柄

    glfw_surface() = default;

    glfw_surface(vk::raii::Instance &instance, GLFWwindow *window)
    {
        createSurface(instance, window);
    }

    // NOTE: 通过 glfw 创建窗口表面
    void createSurface(vk::raii::Instance &instance, GLFWwindow *window)
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }
};

// NOTE: 交换链本质是存放等待投放到屏幕的图像的队列，帧缓冲区 概念的实现
struct glfw_swapchain
{
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;      // NOTE: 存放实际图像
    vk::SurfaceFormatKHR swapChainSurfaceFormat; // NOTE: 表面格式
    vk::Extent2D swapChainExtent;                // NOTE: 基本表面功能

    // 图像视图实际上是图像的视图。它描述了如何访问图像以及访问图像的哪一部分
    std::vector<vk::raii::ImageView> swapChainImageViews; // NOTE: 图像视图

    glfw_swapchain() = default;
    glfw_swapchain(vk::raii::PhysicalDevice &physicalDevice, vk::raii::Device &device,
                   vk::raii::SurfaceKHR &surface, GLFWwindow *window)
    {
        createSwapChain(physicalDevice, device, surface, window);
    }

    /*
我们基本上需要检查三种属性：
    基本表面功能（交换链中的最小/最大图像数，最小/最大值 图像的宽度和高度）
    表面格式（像素格式、色彩空间）
    可用的演示模式
*/
    // NOTE: 创建交换链
    void createSwapChain(vk::raii::PhysicalDevice &physicalDevice,
                         vk::raii::Device &device, vk::raii::SurfaceKHR &surface,
                         GLFWwindow *window)
    {
        // NOTE: 获得表面功能信息
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        // NOTE: 确定基本表面功能
        swapChainExtent = chooseSwapExtent(surfaceCapabilities, window);
        // NOTE: 确定表面格式
        swapChainSurfaceFormat =
            chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));

        // NOTE:填写交换链的创建信息
        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = *surface,
            .minImageCount =
                chooseSwapMinImageCount(surfaceCapabilities), // NOTE: 图像大小
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = // NOTE: 选出展示模式。根据物理设备的功能选出最好的
            chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(*surface)),
            .clipped = true};

        // NOTE: 创建符号配置信息的交换链
        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages(); // NOTE: 生成的帧缓冲区，分配实际内存
    }

    // NOTE: 创建图像视图
    void createImageViews(vk::raii::Device &device)
    {
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{
            .viewType = vk::ImageViewType::e2D, // NOTE: 我们指定要渲染到 2D 屏幕。3d,1D
            .format = swapChainSurfaceFormat.format,
            // 该字段允许您在周围重排颜色通道
            .components = {.r = {}, .g = {}, .b = {}, .a = {}},
            // 该字段描述了图像的用途是什么以及哪个 应该访问部分图像
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        for (auto image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

  private:
    static uint32_t chooseSwapMinImageCount(
        vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
    {
        constexpr auto min_count = 3u;
        auto minImageCount = std::max(min_count, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) &&
            (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
        std::vector<vk::SurfaceFormatKHR> const &availableFormats)
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
        // NOTE: 有限是 信箱模式展示，实时性更好。队列模式可能做无用功
        return std::ranges::any_of(availablePresentModes,
                                   [](const vk::PresentModeKHR value) {
                                       return vk::PresentModeKHR::eMailbox == value;
                                   })
                   ? vk::PresentModeKHR::eMailbox
                   : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities,
                                  GLFWwindow *window)
    {
        if (capabilities.currentExtent.width != 0xFFFFFFFF)
        {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // NOTE: clamp 保证了宽高的结果在合理的范围
        return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width),
                std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height)};
    }
};

struct glfw_context
{
    GLFWwindow *window = nullptr;

    vk::raii::Context context;
    glfw_instance instance;
    physical_device physicalDevice;
    logical_device logicalDevice;

    glfw_surface surface;
    glfw_swapchain swapchain;

    // NOTE: 搭建基本窗口
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }
    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }
    // NOTE: 第一件事是通过创建 一个实例
    void createInstance()
    {
        instance = glfw_instance(context);
    }
    void setupDebugMessenger()
    {
        instance.setupDebugMessenger();
    }
    void pickPhysicalDevice()
    {
        physicalDevice = physical_device(instance.instance);
    }
    void createLogicalDevice()
    {
        logicalDevice = logical_device(physicalDevice.physicalDevice,
                                       physicalDevice.requiredDeviceExtension);
    }
    void createSurface()
    {
        surface = glfw_surface(instance.instance, window);
    }
    void createSwapChain()
    {
        swapchain = glfw_swapchain(physicalDevice.physicalDevice, logicalDevice.device,
                                   surface.surface, window);
    }
    void createImageViews()
    {
        swapchain.createImageViews(logicalDevice.device);
    }
};

class HelloTriangleApplication
{
  public:
    void run()
    {
        ctx.initWindow();

        initVulkan();
        mainLoop();

        ctx.cleanup();
    }

  private:
    glfw_context ctx;

    void initVulkan()
    {
        ctx.createInstance();
        ctx.setupDebugMessenger();

        // NOTE: 创建实例后需要立即创建窗口表面， 因为它实际上会影响物理设备的选择。
        // 还应该注意的是，窗户表面完全是 可选组件，如果你只需要屏幕外渲染
        // Vulkan 还允许您从 不显示 GPU 或通过 Internet 远程，或运行计算
        // 无需渲染或演示目标的 AI 加速
        ctx.createSurface();

        ctx.pickPhysicalDevice();
        ctx.createLogicalDevice();

        ctx.createSwapChain();
        ctx.createImageViews();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(ctx.window))
        {
            glfwPollEvents();
        }
    }
};

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