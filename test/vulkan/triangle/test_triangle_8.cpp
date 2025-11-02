#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>

// NOLINTBEGIN
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

// NOTE:并非所有显卡都能够出于各种原因直接将图像呈现到屏幕上，例如因为它们是为服务器设计的，并且没有任何显示输出。
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
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

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

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
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain; // NOTE: 存储 交换链
    // NOTE: 交换链现在已创建，因此剩下的就是检索其中 VkImage 的句柄
    // 这些图像是由交换链的实现创建的，它们将在交换链销毁后自动清理，因此我们不需要添加任何清理代码。
    std::vector<VkImage> swapChainImages;

    // NOTE: 交换链图像选择的格式和范围
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

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
        createSwapChain(); // NOTE: 创建交换链
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
        // NOTE: 清理交换链
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

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
        // NOTE: 显式启用设备扩展
        // createInfo.enabledExtensionCount = 0 是关闭扩展
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

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

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr,
                                         &debugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void createSurface()
    {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto &device : devices)
        {
            if (isDeviceSuitable(device))
            {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice()
    {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                                  indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    /**
     * @brief
     * 仅检查交换链是否可用是不够的，因为它实际上可能与我们的窗口表面不兼容。
     * 创建交换链还涉及到比实例和设备创建更多的设置，因此我们需要查询更多详细信息才能继续。
     *
     */
    void createSwapChain()
    {
        /**
      我们基本上需要检查三种属性
          基本表面功能（交换链中图像的最小/最大数量，图像的最小/最大宽度和高度）
          表面格式（像素格式，颜色空间）
          可用的演示模式
       *
       */
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        // NOTE: 如果满足了 swapChainAdequate条件，则说明支持绝对足够，但可能仍然存在
        // 许多不同的优化模式。我们现在编写几个函数来找到最佳交换链的正确设置
        /**
        需要确定三种类型的设置：
            表面格式（颜色深度）
            呈现模式（将图像“交换”到屏幕的条件）
            交换范围（交换链中图像的分辨率）
         */
        VkSurfaceFormatKHR surfaceFormat =
            chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode =
            chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // NOTE: 除了这些属性之外，我们还必须决定在交换链中想要拥有多少个图像
        // 仅坚持此最小值意味着我们有时可能必须等待驱动程序完成内部操作，然后才能获取另一个图像进行渲染。
        // 因此，建议请求至少比最小值多一个图像
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        // 我们还应确保在此过程中不要超过最大图像数量，其中 0 是一个特殊值，表示没有最大值
        if (swapChainSupport.capabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // NOTE: 按照 Vulkan 对象的惯例，创建交换链对象需要填写一个大型结构
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        // 指定交换链应绑定到的表面之后，将指定交换链图像的详细信息
        // imageArrayLayers 指定每个图像包含的层数。 除非您正在开发立体 3D
        // 应用程序，否则此值始终为 1。
        // imageUsage 位字段指定我们将在交换链中使用图像进行哪种类型的操作
        // 您可以使用类似 VK_IMAGE_USAGE_TRANSFER_DST_BIT
        // 的值，并使用内存操作将渲染的图像传输到交换链图像
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // 指定如何处理将在多个队列族之间使用的交换链图像
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                         indices.presentFamily.value()};

        // VK_SHARING_MODE_EXCLUSIVE：一个图像一次只能由一个队列族拥有，并且必须在另一个队列族中使用它之前显式地转移所有权。此选项提供最佳性能。
        // VK_SHARING_MODE_CONCURRENT：图像可以在多个队列族之间使用，无需显式的所有权转移。
        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            // 如果图形队列族和呈现队列族相同，这在大多数硬件上都是如此，那么我们应该坚持使用独占模式，因为并发模式要求你指定至少两个不同的队列族。
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        // 可以设置对交换链中的图像应用某些变换,如果支持
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // 指定是否应使用 alpha 通道与窗口系统中的其他窗口进行混合
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        // clipped 成员设置为 VK_TRUE，则意味着我们不关心被遮挡像素的颜色
        createInfo.clipped = VK_TRUE;

        // 现在，我们假设我们只会创建一个交换链. 如果发生失败需要从头开始重新创建交换链
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);

        // NOTE: 检索交换链图像. imageCount 是初始值
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    // NOTE: 表面格式
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &availableFormats)
    {
        // NOTE: format 成员指定颜色通道和类型
        // NOTE: colorSpace 成员指示是否支持 sRGB 色彩空间
        for (const auto &availableFormat : availableFormats)
        {
            // VK_FORMAT_B8G8R8A8_SRGB 表示我们按顺序存储 B、G、R 和 alpha
            // 通道，每个通道使用 8 位无符号整数，每个像素总共 32 位。 colorSpace
            // 成员指示是否支持 sRGB 色彩空间
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        // NOTE: 如果这也失败了,随便选一个
        return availableFormats[0];
    }

    // NOTE: 呈现模式可以说是交换链最重要的设置，因为它表示将图像显示到屏幕的实际条件。
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR> &availablePresentModes)
    {
        // VK_PRESENT_MODE_IMMEDIATE_KHR：应用程序提交的图像会立即传输到屏幕，这可能会导致撕裂
        // VK_PRESENT_MODE_FIFO_KHR：交换链是一个队列，显示器会在刷新时从队列的前面获取图像，程序会将渲染的图像插入队列的后面。
        // 如果队列已满，则程序必须等待。 这与现代游戏中发现的垂直同步最相似。
        // 显示器刷新的时刻称为“垂直消隐”。
        // VK_PRESENT_MODE_FIFO_RELAXED_KHR：如果应用程序迟到且队列在上次垂直消隐时为空，则此模式与前一种模式的区别仅在于此。
        // 当图像最终到达时，它会立即传输，而不是等待下一个垂直消隐。
        // 这可能会导致明显的撕裂。
        // VK_PRESENT_MODE_MAILBOX_KHR：这是第二种模式的另一种变体。
        // 当队列已满时，不是阻塞应用程序，而是简单地将已排队的图像替换为较新的图像。
        // 此模式可用于尽可能快地渲染帧，同时仍避免撕裂，从而导致比标准垂直同步更少的延迟问题。
        // 这通常被称为“三重缓冲”，尽管仅存在三个缓冲区并不一定意味着帧率已解锁。
        for (const auto &availablePresentMode : availablePresentModes)
        {
            // 如果不在意能源消耗，VK_PRESENT_MODE_MAILBOX_KHR 是一个非常好的折衷方案。
            // 它允许我们避免撕裂，同时通过渲染尽可能最新的新图像，直到垂直消隐，仍然保持相当低的延迟。
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }

        // 在能源消耗更为重要的移动设备上，您可能希望改用 VK_PRESENT_MODE_FIFO_KHR
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // NOTE: 交换范围.
    // 交换范围是交换链图像的分辨率，并且几乎总是与我们正在绘制到的窗口的分辨率以像素为单位完全相同
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
    {
        /**
         我们之前在创建窗口时指定的 {WIDTH, HEIGHT} 分辨率是以屏幕坐标为单位测量的。 但是
         Vulkan 使用像素，因此交换链范围也必须以像素为单位指定。 不幸的是，如果您使用高
         DPI 显示器（如 Apple 的 Retina 显示器），屏幕坐标与像素不对应。
         相反，由于较高的像素密度，窗口以像素为单位的分辨率将大于以屏幕坐标为单位的分辨率。
         因此，如果 Vulkan 没有为我们修复交换范围，我们不能只使用原始的 {WIDTH, HEIGHT}。
         相反，我们必须使用 glfwGetFramebufferSize
         来查询窗口以像素为单位的分辨率，然后再将其与最小和最大图像范围进行匹配。
         */
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                       static_cast<uint32_t>(height)};

            // clamp 函数用于将width和height的值限制在实现支持的允许的最小和最大范围之间
            actualExtent.width =
                std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                           capabilities.maxImageExtent.width);
            actualExtent.height =
                std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                           capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // NOTE: 查询支持的表面格式
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                                 details.formats.data());
        }

        // NOTE: 查询支持的演示模式
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                                  nullptr);

        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                                      details.presentModes.data());
        }

        return details;
    }

    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // NOTE: 添加: 验证交换链支持是否足够
        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            // NOTE:至少有一个受支持的图像格式和一个受支持的演示模式，则交换链支持就足够了
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() &&
                                !swapChainSupport.presentModes.empty();
        }

        // NOTE: 我们仅在验证扩展可用后才尝试查询交换链支持
        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    // NOTE: 检查交换链支持
    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                             availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                                 deviceExtensions.end());

        for (const auto &extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                                 queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    std::vector<const char *> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions,
                                             glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers)
        {
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

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

/**
 * @brief Vulkan
 * 没有“默认帧缓冲”的概念，因此它需要一个基础设施来拥有我们将要渲染的缓冲，然后我们才能在屏幕上可视化它们。这个基础设施被称为交换链，必须在
 * Vulkan 中显式创建。交换链本质上是一个等待呈现到屏幕的图像队列。
 *
 * @return int
 */
int main()
{
    // NOTE: 交换链的总体目的是将图像的呈现与屏幕的刷新率同步
    // NOTE: 现在有一组可以绘制并在窗口上显示的图像
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