
#include <cassert>

#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

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
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat,
                                                    offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat,
                                                    offsetof(Vertex, color))};
    }
};

// const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
//                                       {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
//                                       {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
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

    // 缓冲区应该可用于渲染命令，直到程序结束，并且它不依赖于交换链
    vk::raii::Buffer vertexBuffer = nullptr; // NOTE: 顶点缓冲区句柄
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;

    vk::raii::CommandPool commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphore;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphore;
    std::vector<vk::raii::Fence> inFlightFences;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    std::vector<const char *> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName, vk::KHRCreateRenderpass2ExtensionName};

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
    {
        auto app = reinterpret_cast<HelloTriangleApplication *>(
            glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
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
        createVertexBuffer();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }

        device.waitIdle();
    }

    void cleanupSwapChain()
    {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();

        cleanupSwapChain();
        createSwapChain();
        createImageViews();
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

        // query for required features (Vulkan 1.1 and 1.3)
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                {},                             // vk::PhysicalDeviceFeatures2
                {.shaderDrawParameters = true}, // vk::PhysicalDeviceVulkan11Features
                {.synchronization2 = true,
                 .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
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
            createShaderModule(readFile("shaders/18_shader_vertexbuffer.spv"));

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

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount =
                static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data()};
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

    void createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueIndex};
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    // NOTE: 创建顶点缓冲区
    void createVertexBuffer()
    {
        // NOTE: 1. 缓冲区配置
        //  flags，用于配置稀疏缓冲内存
        //  size，它指定缓冲区的大小（以字节为单位）
        //  usage，它指示缓冲区中的数据将用于哪些目的
        //  sharingMode,就像交换链中的图像一样，缓冲区也可以由特定的队列家族拥有，或者同时在多个队列之间共享。
        //  缓冲区只会从图形队列中使用，所以我们可以坚持独占访问
        vk::BufferCreateInfo bufferInfo{.flags = {},
                                        .size = sizeof(vertices[0]) * vertices.size(),
                                        .usage = vk::BufferUsageFlagBits::eVertexBuffer,
                                        .sharingMode = vk::SharingMode::eExclusive};

        vertexBuffer = vk::raii::Buffer(device, bufferInfo);

        // NOTE: 2. 内存需求
        vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
        // size：所需内存的大小（以字节为单位）可能与bufferInfo.size不同
        // alignment：缓冲区在分配的内存区域中开始的偏移量（以字节为单位）取决于bufferInfo.usage和bufferInfo.flags。
        // memoryTypeBits：适合缓冲区的内存类型的位字段。
        vk::MemoryAllocateInfo memoryAllocateInfo{
            .allocationSize = memRequirements.size,
            .memoryTypeIndex =
                findMemoryType(memRequirements.memoryTypeBits,
                               vk::MemoryPropertyFlagBits::eHostVisible |
                                   vk::MemoryPropertyFlagBits::eHostCoherent)};
        // NOTE: 内存分配
        vertexBufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);

        // NOTE: 4. 内存与缓冲区关联
        vertexBuffer.bindMemory(*vertexBufferMemory, 0);

        // NOTE: 5. 填充顶点缓冲区

        /*
        此函数允许我们访问由偏移量和大小定义的指定内存资源的区域。这里的偏移量和大小分别为0和bufferInfo.size
        驱动程序可能不会立即将数据复制到缓冲内存中，例如，因为缓存。也有可能对缓冲区的写入在映射内存中还不可见。
        有两种方法可以处理这个问题
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                使用主机相干的内存堆，用VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        写入映射内存后调用vkFlushMappedMemoryRanges，从映射内存读取前调用vkInvalidateMappedMemoryRanges

        我们采用了第一种方法，它确保映射的内存始终与分配内存的内容匹配。请记住，这可能会导致比显式刷新稍差的性能，但是我们将在下一章中看到为什么这并不重要。

        刷新内存范围或使用连贯的内存堆意味着驱动程序将知道我们对缓冲区的写入，但这并不意味着它们在GPU上实际上是可见的。将数据传输到GPU是在后台发生的操作，规范只是告诉我们，它保证在下一次调用vkQueueSubmit时完成。
        */
        void *data = vertexBufferMemory.mapMemory(0, bufferInfo.size, {});
        // 简单地将顶点数据memcpy到映射内存，然后使用vkUnmapMemory再次取消映射
        memcpy(data, vertices.data(), bufferInfo.size);
        vertexBufferMemory.unmapMemory();
    }

    // NOTE: 3. 找到合适的内存类型
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        // 查询可用内存类型的信息
        vk::PhysicalDeviceMemoryProperties memProperties =
            physicalDevice.getMemoryProperties();

        /*
这个VkPhysicalDeviceMemoryProperties结构有两个数组，memoryTypes和memoryHeaps。
内存堆是不同的内存资源，比如专用的VRAM和在VRAM用完时交换RAM中的空间。这些堆中存在不同类型的内存。
现在我们只关心内存的类型，而不是它来自什么堆，但是你可以想象这会影响性能。
*/
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createCommandBuffers()
    {
        commandBuffers.clear();
        vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount =
                                                    MAX_FRAMES_IN_FLIGHT};
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    }

    void recordCommandBuffer(uint32_t imageIndex)
    {
        commandBuffers[currentFrame].begin({});
        // Before starting rendering, transition the swapchain image to
        // COLOR_ATTACHMENT_OPTIMAL
        transition_image_layout(
            imageIndex, vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {}, // srcAccessMask (no need to wait for previous operations)
            vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
            vk::PipelineStageFlagBits2::eColorAttachmentOutput  // dstStage
        );
        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColor};
        vk::RenderingInfo renderingInfo = {
            .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentInfo};
        commandBuffers[currentFrame].beginRendering(renderingInfo);
        commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                  *graphicsPipeline);
        commandBuffers[currentFrame].setViewport(
            0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                            static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffers[currentFrame].setScissor(
            0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

        // NOTE: 6. 绑定顶点缓冲区
        commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, {0});
        // 您还应该更改对vkCmdDraw的调用，以传递缓冲区中的顶点数，而不是硬编码数3
        commandBuffers[currentFrame].draw(static_cast<uint32_t>(vertices.size()), 1, 0,
                                          0);

        commandBuffers[currentFrame].endRendering();
        // After rendering, transition the swapchain image to PRESENT_SRC
        transition_image_layout(
            imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
            {},                                                 // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
            vk::PipelineStageFlagBits2::eBottomOfPipe           // dstStage
        );
        commandBuffers[currentFrame].end();
    }

    void transition_image_layout(uint32_t imageIndex, vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout,
                                 vk::AccessFlags2 src_access_mask,
                                 vk::AccessFlags2 dst_access_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask)
    {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = src_stage_mask,
            .srcAccessMask = src_access_mask,
            .dstStageMask = dst_stage_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapChainImages[imageIndex],
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};
        vk::DependencyInfo dependency_info = {.dependencyFlags = {},
                                              .imageMemoryBarrierCount = 1,
                                              .pImageMemoryBarriers = &barrier};
        commandBuffers[currentFrame].pipelineBarrier2(dependency_info);
    }

    void createSyncObjects()
    {
        presentCompleteSemaphore.clear();
        renderFinishedSemaphore.clear();
        inFlightFences.clear();

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            presentCompleteSemaphore.emplace_back(device, vk::SemaphoreCreateInfo());
            renderFinishedSemaphore.emplace_back(device, vk::SemaphoreCreateInfo());
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            inFlightFences.emplace_back(
                device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
        }
    }

    void drawFrame()
    {
        while (vk::Result::eTimeout ==
               device.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX))
            ;
        auto [result, imageIndex] = swapChain.acquireNextImage(
            UINT64_MAX, *presentCompleteSemaphore[semaphoreIndex], nullptr);

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChain();
            return;
        }
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        device.resetFences(*inFlightFences[currentFrame]);
        commandBuffers[currentFrame].reset();
        recordCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitDestinationStageMask(
            vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*presentCompleteSemaphore[semaphoreIndex],
            .pWaitDstStageMask = &waitDestinationStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffers[currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*renderFinishedSemaphore[imageIndex]};
        queue.submit(submitInfo, *inFlightFences[currentFrame]);

        try
        {
            const vk::PresentInfoKHR presentInfoKHR{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &*renderFinishedSemaphore[imageIndex],
                .swapchainCount = 1,
                .pSwapchains = &*swapChain,
                .pImageIndices = &imageIndex};
            result = queue.presentKHR(presentInfoKHR);
            if (result == vk::Result::eErrorOutOfDateKHR ||
                result == vk::Result::eSuboptimalKHR || framebufferResized)
            {
                framebufferResized = false;
                recreateSwapChain();
            }
            else if (result != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to present swap chain image!");
            }
        }
        catch (const vk::SystemError &e)
        {
            if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR))
            {
                recreateSwapChain();
                return;
            }
            else
            {
                throw;
            }
        }
        semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
        // NOTE: 顶点缓冲区创建
        // NOTE: Vulkan中的缓冲区是用于 存储图形卡 可以读取的任意数据的内存区域。
        // NOTE: 缓冲区不会自动为自己分配内存。
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