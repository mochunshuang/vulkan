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
    uint32_t queueIndex = ~0;
    vk::raii::Queue queue = nullptr;
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR swapChainSurfaceFormat;
    vk::Extent2D swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::CommandPool commandPool = nullptr;     // NOTE: 命令池
    vk::raii::CommandBuffer commandBuffer = nullptr; // NOTE: 命令缓冲区

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
        createCommandPool();
        createCommandBuffer();
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
        vk::raii::ShaderModule shaderModule =
            createShaderModule(readFile("shaders/09_shader_base.spv"));

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = "vertMain"};
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = "fragMain"};
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                            fragShaderStageInfo};

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList};
        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                          .scissorCount = 1};

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .depthBiasSlopeFactor = 1.0f,
            .lineWidth = 1.0f};

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False};

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = vk::False,
            .colorWriteMask =
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

        vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable = vk::False,
                                                            .logicOp = vk::LogicOp::eCopy,
                                                            .attachmentCount = 1,
                                                            .pAttachments =
                                                                &colorBlendAttachment};

        std::vector dynamicStates = {vk::DynamicState::eViewport,
                                     vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0,
                                                        .pushConstantRangeCount = 0};

        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                           vk::PipelineRenderingCreateInfo>
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
                 .renderPass = nullptr},
                {.colorAttachmentCount = 1,
                 .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

        graphicsPipeline = vk::raii::Pipeline(
            device, nullptr,
            pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    }

    // NOTE: 创建命令池
    void createCommandPool()
    {
        /*
        创建命令池仅采用两个参数：
        有两个可能的标志：
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT：
            提示命令缓冲区经常使用新命令重新记录（可能会更改内存分配行为）
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT：
            允许单独重新记录命令缓冲区，如果没有此标志，它们必须一起重置
        */
        // 命令缓冲区是通过在其中一个设备队列上【提交】命令缓冲区来执行的
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueIndex};
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    // NOTE: 分配单个命令缓冲区
    void createCommandBuffer()
    {
        // 指定要分配的命令池和缓冲区数量
        // level: 确定是主命令缓冲区还是辅助命令缓冲区。
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY：可以提交到队列执行，但不能从其他命令缓冲区调用。
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY：不能直接提交，但可以从主命令缓冲区调用。
        vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1};
        commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
    }

    // NOTE: 命令缓冲区记录： 将要执行的命令写入到命令缓冲区的函数
    void recordCommandBuffer(uint32_t imageIndex)
    {
        // imageIndex: 我们要写入的当前交换链镜像的索引

        // 总是通过调用一个小结构作为参数来开始记录命令缓冲区
        /*
flags:
VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT：命令缓冲区执行一次后将立即重新记录。
VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT：这是一个辅助命令缓冲区，将完全位于单个渲染通道中。
VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT：命令缓冲区可以在已处于待执行状态时重新提交。

这些标志目前都不适用于我们。
该参数仅与辅助命令缓冲区相关.它指定要从调用主命令缓冲区继承的状态。
*/
        // NOTE: 0: 开始记录命令缓冲区
        commandBuffer.begin({.flags = {}});

        // 转换图像布局以进行渲染
        // Transition the image layout for rendering
        transition_image_layout(
            imageIndex, vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal, // NOTE:最佳的彩色附件
            {}, //// srcAccessMask (no need to wait for previous operations)
            vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
            vk::PipelineStageFlagBits2::eColorAttachmentOutput  // dstStage
        );

        // NOTE: 启动动态渲染
        // NOTE: 我们不需要创建渲染通道或帧缓冲区。相反，我们在开始渲染时直接指定附件
        // Set up the color attachment
        // NOTE: 在渲染之前将图像清除为黑色。图像渲染之前演示通过clearColor指定
        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal, // 颜色附件
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColor};

        // NOTE: 设置渲染信息：
        // Set up the rendering info
        vk::RenderingInfo renderingInfo = {
            // 渲染区域的大小
            .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount = 1,           // 层数为 1
            .colorAttachmentCount = 1, // 颜色附件
            .pColorAttachments = &attachmentInfo};

        // NOTE: 1: 设置附件就可以开始渲染了：
        //  Begin rendering
        commandBuffer.beginRendering(renderingInfo);

        // Rendering commands will go here
        // NOTE: 2: 基本绘图命令
        // 第二个参数指定管道对象是图形管道还是计算管道。
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

        // 我们确实为该管道指定了动态的视口和剪刀状态。 因此，在发出 draw
        // 命令之前，我们需要在命令缓冲区中设置它们
        commandBuffer.setViewport(
            0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                            static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

        /*
        现在我们准备好为三角形发出绘制命令：
        实际功能有点虎头蛇尾，但由于我们提前指定了所有信息，所以它非常简单。
        vertexCount：即使我们没有顶点缓冲区，从技术上讲，我们仍然有 3 个顶点可供绘制。
        instanceCount：用于实例化渲染，如果您不这样做，请使用 1
        firstVertex：用作顶点缓冲区的偏移量，定义 的最小值。SV_VertexId
        firstInstance：用作实例化渲染的偏移量，定义 的最小值。SV_InstanceID
        */
        commandBuffer.draw(3, 1, 0, 0);

        //  End rendering
        // NOTE: 3: 现在可以结束渲染
        commandBuffer.endRendering();

        // NOTE: 4: 渲染后，我们需要将图像布局转换回 ，以便将其呈现在屏幕上：
        //  Transition the image layout for presentation
        transition_image_layout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
                                vk::ImageLayout::ePresentSrcKHR,
                                vk::AccessFlagBits2::eColorAttachmentWrite, {},
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                vk::PipelineStageFlagBits2::eBottomOfPipe);

        // NOTE: 5: 完成命令缓冲区的记录。
        commandBuffer.end();

        // NOTE: 要通过交换链 得到渲染的图像，才能呈现
    }

    // NOTE: 图像布局过渡: 在开始渲染到图像之前，我们需要将其布局转换为适合渲染的布局。
    void transition_image_layout(uint32_t imageIndex, vk::ImageLayout oldLayout,
                                 vk::ImageLayout newLayout,
                                 vk::AccessFlags2 srcAccessMask,
                                 vk::AccessFlags2 dstAccessMask,
                                 vk::PipelineStageFlags2 srcStageMask,
                                 vk::PipelineStageFlags2 dstStageMask)
    {
        // NOTE: 此功能将用于在渲染前后转换图像布局。
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = srcStageMask,
            .srcAccessMask = srcAccessMask,
            .dstStageMask = dstStageMask,
            .dstAccessMask = dstAccessMask,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapChainImages[imageIndex],
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};
        vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                             .imageMemoryBarrierCount = 1,
                                             .pImageMemoryBarriers = &barrier};
        commandBuffer.pipelineBarrier2(dependencyInfo);
    }

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(
        const std::vector<char> &code) const
    {
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
};

int main()
{
    try
    {
        // NOTE: Vulkan 中的命令，如绘图作和内存传输，不是直接使用函数调用执行的。
        // 您必须在命令缓冲区对象中记录要执行的所有操作
        // 这样做的好处是，所有命令都一起提交。
        // NOTE: 我们必须先创建一个命令池，然后才能创建命令缓冲区。
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