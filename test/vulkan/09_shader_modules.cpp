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

    void createGraphicsPipeline()
    {
        // NOTE: 着色器模块创建
        vk::raii::ShaderModule shaderModule =
            createShaderModule(readFile("shaders/09_shader_base.spv"));

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
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(
        const std::vector<char> &code) const
    {
        // 创建一个着色器模块很简单，我们只需要用字节码和它的长度指定一个指向缓冲区的指针
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t *>(code.data())};
        vk::raii::ShaderModule shaderModule{device, createInfo};

        return shaderModule;
    }

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
        // NOTE: 生成一个SPIR-V二进制文件并将其加载到程序中
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