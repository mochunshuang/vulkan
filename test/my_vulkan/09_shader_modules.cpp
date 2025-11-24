
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

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

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

static std::vector<char> readFile(const std::string &filename)
{
    // ate：在文件末尾开始读取
    // binary：将文件作为二进制文件读取（避免文本转换）
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }
    // 在文件末尾开始读取的好处是我们可以使用读取位置来确定文件的大小并分配一个缓冲区：
    std::vector<char> buffer(file.tellg());
    // 之后，我们可以回到文件的开头并一次读取所有字节：
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
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
    vk::raii::Device device = nullptr; // NOTE: 逻辑设备
    vk::raii::Queue queue = nullptr;   // NOTE: 图形队列

    logical_device() = default;

    logical_device(vk::raii::PhysicalDevice &physicalDevice,
                   std::vector<const char *> &requiredDeviceExtension,
                   vk::raii::SurfaceKHR &surface)
    {
        createLogicalDevice(physicalDevice, requiredDeviceExtension, surface);
    }

    void createLogicalDevice(vk::raii::PhysicalDevice &physicalDevice,
                             std::vector<const char *> &requiredDeviceExtension,
                             vk::raii::SurfaceKHR &surface)
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
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                {},                             // vk::PhysicalDeviceFeatures2
                {.shaderDrawParameters = true}, // vk::PhysicalDeviceVulkan11Features
                {.dynamicRendering = true},     // vk::PhysicalDeviceVulkan13Features
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

            // NOTE: 增添 PhysicalDeviceVulkan11Features 要求 来适配 shader
            auto features = device.template getFeatures2<
                vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures =
                features.template get<vk::PhysicalDeviceVulkan11Features>()
                    .shaderDrawParameters &&
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
        logicalDevice =
            logical_device(physicalDevice.physicalDevice,
                           physicalDevice.requiredDeviceExtension, surface.surface);
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
    // NOTE: 创建管线。
    void createGraphicsPipeline()
    {
        // NOTE: 着色器模块创建
        vk::raii::ShaderModule shaderModule = createShaderModule(
            readFile("shaders/09_shader_base.spv"), logicalDevice.device);

        // 配置阶段的描述信息
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex, // 顶点着色阶段
            .module = shaderModule,
            .pName = "vertMain"}; // NOTE: 09_shader_base.slang 顶点入口函数
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment, // 片段着色阶段
            .module = shaderModule,                      // 指定绑定到的 着色器模块
            .pName = "fragMain"};
        // NOTE: 着色器阶段创建：只有两个
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                            fragShaderStageInfo};

        // NOTE: 这就是描述管道的可编程阶段的全部内容. 这里仅仅配置两个
    }

    // NOTE: 创建着色器模块
    [[nodiscard]] static vk::raii::ShaderModule createShaderModule(
        const std::vector<char> &code, vk::raii::Device &device)
    {
        // 创建一个着色器模块很简单，我们只需要用字节码和它的长度指定一个指向缓冲区的指针
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t *>(code.data())};
        vk::raii::ShaderModule shaderModule{device, createInfo};

        return shaderModule;
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

        ctx.createGraphicsPipeline();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(ctx.window))
        {
            glfwPollEvents();
        }
    }
};

/*
[顶点着色器]处理每个输入的顶点。
它以世界坐标位置、颜色、法线和纹理坐标等属性作为输入。
其输出是裁剪空间中的最终位置，以及需要传递给片段着色器的属性，如颜色和纹理坐标。
随后，这些值将由[光栅化器]在各个片段之间进行插值，以生成平滑的渐变效果。

裁剪坐标是来自顶点着色器（vertex
shader）的四维向量，后续会通过将整个向量除以其最后一个分量，转换为标准化设备坐标（normalized
device coordinate）。这些标准化设备坐标属于齐次坐标（homogeneous
coordinates），它们会将帧缓冲区（framebuffer）映射到一个[-1, 1] × [-1,1]的坐标系
//NOTE: 中心点从 [x,y] -> [0,0]
*/
int main()
{
    try
    {
        // NOTE: 修改 设备，增加新特性要求。要求队列 同时支持图形和显示的队列
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