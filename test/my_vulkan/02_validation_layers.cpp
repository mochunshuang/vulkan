
// NOTE: 没有这一行宏，createInstance 失败
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

// NOTE: 获取实例扩展信息的 公共函数
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

auto setupDebugMessenger(vk::raii::Instance &instance,
                         vk::raii::DebugUtilsMessengerEXT &debugMessenger)
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

struct glfw_context
{
    GLFWwindow *window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

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
        ::setupDebugMessenger(instance, debugMessenger);
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