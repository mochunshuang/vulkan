#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>

// NOLINTBEGIN
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// NOTE: 创建调试信使的扩展函数封装
// 由于调试扩展不是 Vulkan 核心的一部分，需要通过 vkGetInstanceProcAddr 动态加载
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    // 动态获取函数指针
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// NOTE: 销毁调试信使的扩展函数封装
void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

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
    GLFWwindow *window;

    VkInstance instance;
    // NOTE: 在 Vulkan 中，调试回调也是用需要显式创建和销毁的句柄来管理的
    VkDebugUtilsMessengerEXT debugMessenger;

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
        setupDebugMessenger(); // NOTE: 新增：设置调试回调
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
        // NOTE: 新增：清理调试信使
        if (enableValidationLayers)
        {
            // NOTE:删除对DestroyDebugUtilsMessengerEXT的调用，并运行你的程序,会出错
            // NOTE: 调试实例销毁
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

// #define TEST_ERROR
#ifndef TEST_ERROR
    void createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // NOTE: 修改：使用 getRequiredExtensions() 函数获取扩展列表
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // NOTE: vk_layer_settings.txt 可以更细节的配置
        // NOTE: 新增：调试信使创建信息，用于实例创建期间的调试
        // debugCreateInfo 变量放置在 if 语句之外，以确保它在 vkCreateInstance
        // 调用之前不会被销毁。通过以这种方式创建额外的调试信使，它将在 vkCreateInstance
        // 和 vkDestroyInstance 期间自动使用，并在之后进行清理。
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // 首先将信使创建信息的填充提取到一个单独的函数中
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }
#else
    void createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;

            // NOTE: 故意制造验证层错误 - 设置错误的消息严重性标志
            // 使用无效的标志组合会触发验证层警告
            debugCreateInfo.messageSeverity = 0xFFFFFFFF; // 无效的标志位
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }
#endif

    // NOTE: 新增：填充调试信使创建信息的辅助函数
    // 此结构应传递给 vkCreateDebugUtilsMessengerEXT 函数以创建 VkDebugUtilsMessengerEXT
    // 对象。不幸的是，由于此函数是扩展函数，因此不会自动加载。我们必须使用
    // vkGetInstanceProcAddr 自己查找其地址。
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        // 设置要接收的消息严重级别：
        // VERBOSE: 详细诊断信息
        // WARNING: 警告信息
        // ERROR: 错误信息
        /*
        第一个参数指定消息的严重程度，它是以下标志之一
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT：诊断消息
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT：信息性消息，例如资源的创建
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT：关于不一定是错误的行为的消息，但很可能是应用程序中的错误
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT：关于无效且可能导致崩溃的行为的消息
        */
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        // if (createInfo.messageSeverity >=
        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        // {
        //     // Message is important enough to show
        // }

        // 设置要接收的消息类型：
        // GENERAL: 与规范或性能无关的一般事件
        // VALIDATION: 违反规范或可能错误的使用
        // PERFORMANCE: 潜在的性能问题
        /*
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT：发生了一些与规范或性能无关的事件
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT：发生了一些违反规范或指示可能错误的事情
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT：Vulkan 的潜在非最佳使用
        */
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback; // 设置回调函数
        createInfo.pUserData = nullptr;             // Optional
    }

    // NOTE: 调试实例的创建
    void setupDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        // 使用扩展函数创建调试信使
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr,
                                         &debugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    // NOTE: 新增：获取所需扩展的辅助函数
    // 要在程序中设置回调以处理消息和相关详细信息，我们必须使用 VK_EXT_debug_utils
    // 扩展设置一个带有回调的调试消息传递器
    std::vector<const char *> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // 将 GLFW 返回的扩展数组转换为 vector
        std::vector<const char *> extensions(glfwExtensions,
                                             glfwExtensions + glfwExtensionCount);

        // NOTE: 如果启用验证层，添加调试工具扩展
        if (enableValidationLayers)
        {
            // 等于文字字符串“VK_EXT_debug_utils”。使用此宏可让您避免拼写错误
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    // NOTE: 新增：调试回调函数 - 当验证层有消息时会被调用
    // VKAPI_ATTR 和 VKAPI_CALL 确保函数具有正确的签名以供 Vulkan 调用
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, // 消息严重级别
                  VkDebugUtilsMessageTypeFlagsEXT messageType,            // 消息类型
                  const VkDebugUtilsMessengerCallbackDataEXT
                      *pCallbackData, // 回调数据，包含具体消息
                  void *pUserData)    // 用户自定义数据指针
    {
        /*
        pCallbackData 重要成员：
                pMessage：作为空终止字符串的调试消息
                pObjects：与消息相关的 Vulkan 对象句柄数组
                objectCount：数组中的对象数

        pUserData 参数包含一个在回调设置期间指定的指针，允许您将自己的数据传递给它
        */
        // 简单地将验证层消息输出到标准错误流
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        // 回调返回一个布尔值，指示是否应中止触发验证层消息的 Vulkan 调用。如果回调返回
        // true，则调用将以 VK_ERROR_VALIDATION_FAILED_EXT
        // 错误中止。这通常仅用于测试验证层本身，因此您应始终返回 VK_FALSE
        return VK_FALSE;
    }
};

// NOTE: 验证层的相关设置代码
int main()
{
    HelloTriangleApplication app;

    try
    {
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