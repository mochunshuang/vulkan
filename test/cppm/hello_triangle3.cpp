/* Copyright (c) 2025, NVIDIA CORPORATION. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>
#include <print>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <optional>
#include <set>
#include <algorithm>
#include <ranges>

// 简单的日志宏

#define LOGI(...) std::println(__VA_ARGS__)
#define LOGW(...) std::println(__VA_ARGS__)
#define LOGE(...) std::println(__VA_ARGS__)
#define LOGD(...) std::println(__VA_ARGS__)

/**
 * @brief A self-contained GLFW+Vulkan sample that illustrates
 * the rendering of a triangle using Vulkan 1.3
 * This version closely follows the original vkb sample structure
 */
class HPPHelloTriangleV13
{
  private:
    // Define the Vertex structure
    struct Vertex
    {
        glm::vec2 position;
        glm::vec3 color;
    };

    // Define the vertex data (same as original)
    const std::vector<Vertex> vertices = {
        {{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // Vertex 1: Red
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},  // Vertex 2: Green
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}  // Vertex 3: Blue
    };

    /**
     * @brief Swapchain state (same as original)
     */
    struct SwapchainDimensions
    {
        uint32_t width = 0;
        uint32_t height = 0;
        vk::Format format = vk::Format::eUndefined;
    };

    /**
     * @brief Per-frame data (same as original)
     */
    struct PerFrame
    {
        vk::Fence queue_submit_fence = nullptr;
        vk::CommandPool primary_command_pool = nullptr;
        vk::CommandBuffer primary_command_buffer = nullptr;
        vk::Semaphore swapchain_acquire_semaphore = nullptr;
        vk::Semaphore swapchain_release_semaphore = nullptr;
    };

    /**
     * @brief Vulkan objects and global state (same as original)
     */
    struct Context
    {
        vk::Instance instance = nullptr;
        vk::PhysicalDevice gpu = nullptr;
        vk::Device device = nullptr;
        vk::Queue queue = nullptr;
        vk::SwapchainKHR swapchain = nullptr;
        SwapchainDimensions swapchain_dimensions;
        vk::SurfaceKHR surface = nullptr;
        uint32_t graphics_queue_index = 0;
        std::vector<vk::ImageView> swapchain_image_views;
        std::vector<vk::Image> swapchain_images;
        vk::Pipeline pipeline = nullptr;
        vk::PipelineLayout pipeline_layout = nullptr;
        vk::DebugUtilsMessengerEXT debug_callback = nullptr;
        std::vector<vk::Semaphore> recycled_semaphores;
        std::vector<PerFrame> per_frame;
        vk::Buffer vertex_buffer = nullptr;
        vk::DeviceMemory vertex_buffer_memory = nullptr;
    };

  public:
    HPPHelloTriangleV13() = default;
    ~HPPHelloTriangleV13()
    {
        cleanup();
    }

    void run()
    {
        prepare();
        mainLoop();
    }

  private:
    GLFWwindow *window = nullptr;
    Context context;
    bool framebufferResized = false;
    const uint32_t WINDOW_WIDTH = 800;
    const uint32_t WINDOW_HEIGHT = 600;

    void prepare()
    {
        initWindow();
        initInstance();
        createSurface();
        selectPhysicalDevice();
        initDevice();
        initVertexBuffer();
        initSwapchain();
        initPipeline();
    }

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                  "Vulkan Triangle 1.3 (GLFW)", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
    {
        auto app =
            reinterpret_cast<HPPHelloTriangleV13 *>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initInstance()
    {
        LOGI("Initializing Vulkan instance.");

        vk::ApplicationInfo app;
        app.pApplicationName = "Hello Triangle V1.3";
        app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app.pEngineName = "No Engine";
        app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app.apiVersion = VK_API_VERSION_1_3;

        // Get required extensions from GLFW
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions =
            glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions,
                                             glfwExtensions + glfwExtensionCount);

        // Add debug utils if available
        auto availableExtensions = vk::enumerateInstanceExtensionProperties();
        bool hasDebugUtils =
            std::ranges::any_of(availableExtensions, [](const auto &ext) {
                return strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
            });

        if (hasDebugUtils)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // Validation layers
        std::vector<const char *> layers;
        auto availableLayers = vk::enumerateInstanceLayerProperties();
        bool hasValidation = std::ranges::any_of(availableLayers, [](const auto &layer) {
            return strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
        });

        if (hasValidation)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            LOGI("Enabled Validation Layer: VK_LAYER_KHRONOS_validation");
        }

        vk::InstanceCreateInfo createInfo;
        createInfo.pApplicationInfo = &app;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();

        // Debug callback setup (similar to original)
        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (hasDebugUtils)
        {
            debugCreateInfo.messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
            debugCreateInfo.messageType =
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debugCreateInfo.pfnUserCallback = debugCallback;
            createInfo.pNext = &debugCreateInfo;
        }

        context.instance = vk::createInstance(createInfo);

        // Create debug callback (similar to original)
        if (hasDebugUtils)
        {
            context.debug_callback =
                context.instance.createDebugUtilsMessengerEXT(debugCreateInfo);
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT,
                  vk::Flags<vk::DebugUtilsMessageTypeFlagBitsEXT>,
                  const vk::DebugUtilsMessengerCallbackDataEXT *, void *)
    {

        return VK_FALSE;
    }

    void createSurface()
    {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(static_cast<VkInstance>(context.instance), window,
                                    nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }
        context.surface = surface;
    }

    void selectPhysicalDevice()
    {
        auto devices = context.instance.enumeratePhysicalDevices();

        for (const auto &device : devices)
        {
            // Check for Vulkan 1.3 support (like original)
            vk::PhysicalDeviceProperties props = device.getProperties();
            if (props.apiVersion < VK_MAKE_VERSION(1, 3, 0))
            {
                LOGW("Physical device '{}' does not support Vulkan 1.3, skipping.",
                     props.deviceName);
                continue;
            }

            // Find queue family (like original)
            auto queueFamilies = device.getQueueFamilyProperties();
            uint32_t i = 0;
            for (const auto &queueFamily : queueFamilies)
            {
                if ((queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) &&
                    device.getSurfaceSupportKHR(i, context.surface))
                {
                    context.gpu = device;
                    context.graphics_queue_index = i;
                    LOGI("Selected physical device: {}", props.deviceName);
                    return;
                }
                i++;
            }
        }

        throw std::runtime_error(
            "Failed to find a suitable GPU with Vulkan 1.3 support.");
    }

    void initDevice()
    {
        LOGI("Initializing Vulkan device.");

        // Check device extensions (like original)
        auto deviceExtensions = context.gpu.enumerateDeviceExtensionProperties();
        std::vector<const char *> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        if (!validateExtensions(requiredExtensions, deviceExtensions))
        {
            throw std::runtime_error("Required device extensions are missing");
        }

        // Query Vulkan 1.3 features (like original)
        auto supportedFeatures =
            context.gpu.getFeatures2<vk::PhysicalDeviceFeatures2,
                                     vk::PhysicalDeviceVulkan13Features,
                                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        // Check required features (like original)
        if (!supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering)
        {
            throw std::runtime_error("Dynamic Rendering feature is missing");
        }
        if (!supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>().synchronization2)
        {
            throw std::runtime_error("Synchronization2 feature is missing");
        }
        if (!supportedFeatures.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
                 .extendedDynamicState)
        {
            throw std::runtime_error("Extended Dynamic State feature is missing");
        }

        // Enable features (like original)
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enabledFeatures = {{},
                               {.synchronization2 = true, .dynamicRendering = true},
                               {.extendedDynamicState = true}};

        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo;
        queueInfo.queueFamilyIndex = context.graphics_queue_index;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.pNext = &enabledFeatures.get<vk::PhysicalDeviceFeatures2>();
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount =
            static_cast<uint32_t>(requiredExtensions.size());
        deviceInfo.ppEnabledExtensionNames = requiredExtensions.data();

        context.device = context.gpu.createDevice(deviceInfo);
        context.queue = context.device.getQueue(context.graphics_queue_index, 0);
    }

    void initSwapchain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCaps =
            context.gpu.getSurfaceCapabilitiesKHR(context.surface);

        // Select surface format (similar to original's
        // vkb::common::select_surface_format)
        auto formats = context.gpu.getSurfaceFormatsKHR(context.surface);
        vk::SurfaceFormatKHR format = formats[0]; // fallback
        for (const auto &availableFormat : formats)
        {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                format = availableFormat;
                break;
            }
        }

        vk::Extent2D extent;
        if (surfaceCaps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            extent = surfaceCaps.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            extent.width =
                std::clamp(static_cast<uint32_t>(width), surfaceCaps.minImageExtent.width,
                           surfaceCaps.maxImageExtent.width);
            extent.height = std::clamp(static_cast<uint32_t>(height),
                                       surfaceCaps.minImageExtent.height,
                                       surfaceCaps.maxImageExtent.height);
        }

        uint32_t imageCount = surfaceCaps.minImageCount + 1;
        if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
        {
            imageCount = surfaceCaps.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo;
        createInfo.surface = context.surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = format.format;
        createInfo.imageColorSpace = format.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.preTransform = surfaceCaps.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode = vk::PresentModeKHR::eFifo; // Like original
        createInfo.clipped = VK_TRUE;

        context.swapchain = context.device.createSwapchainKHR(createInfo);
        context.swapchain_images =
            context.device.getSwapchainImagesKHR(context.swapchain);
        context.swapchain_dimensions = {extent.width, extent.height, format.format};

        // Create image views (like original)
        for (auto image : context.swapchain_images)
        {
            vk::ImageViewCreateInfo viewInfo;
            viewInfo.image = image;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.format = context.swapchain_dimensions.format;
            viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            context.swapchain_image_views.push_back(
                context.device.createImageView(viewInfo));
        }

        // Initialize per-frame resources (like original)
        context.per_frame.resize(context.swapchain_images.size());
        for (auto &per_frame : context.per_frame)
        {
            initPerFrame(per_frame);
        }
    }

    void initPerFrame(PerFrame &per_frame)
    {
        per_frame.queue_submit_fence =
            context.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});

        vk::CommandPoolCreateInfo cmdPoolInfo;
        cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
        cmdPoolInfo.queueFamilyIndex = context.graphics_queue_index;
        per_frame.primary_command_pool = context.device.createCommandPool(cmdPoolInfo);

        vk::CommandBufferAllocateInfo cmdBufInfo;
        cmdBufInfo.commandPool = per_frame.primary_command_pool;
        cmdBufInfo.level = vk::CommandBufferLevel::ePrimary;
        cmdBufInfo.commandBufferCount = 1;
        per_frame.primary_command_buffer =
            context.device.allocateCommandBuffers(cmdBufInfo)[0];
    }

    void initVertexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;

        context.vertex_buffer = context.device.createBuffer(bufferInfo);

        vk::MemoryRequirements memReqs =
            context.device.getBufferMemoryRequirements(context.vertex_buffer);

        vk::MemoryAllocateInfo allocInfo;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible |
                                        vk::MemoryPropertyFlagBits::eHostCoherent);

        context.vertex_buffer_memory = context.device.allocateMemory(allocInfo);
        context.device.bindBufferMemory(context.vertex_buffer,
                                        context.vertex_buffer_memory, 0);

        void *data =
            context.device.mapMemory(context.vertex_buffer_memory, 0, bufferSize);
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        context.device.unmapMemory(context.vertex_buffer_memory);
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        auto memProperties = context.gpu.getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    void initPipeline()
    {
        // Create pipeline layout (like original)
        context.pipeline_layout = context.device.createPipelineLayout({});

        // Vertex input (like original)
        vk::VertexInputBindingDescription bindingDesc;
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(Vertex);
        bindingDesc.inputRate = vk::VertexInputRate::eVertex;

        std::array<vk::VertexInputAttributeDescription, 2> attributeDesc = {
            {{0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)},
             {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)}}};

        vk::PipelineVertexInputStateCreateInfo vertexInput;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDesc.size());
        vertexInput.pVertexAttributeDescriptions = attributeDesc.data();

        // Input assembly (like original)
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

        // Rasterization (like original)
        vk::PipelineRasterizationStateCreateInfo raster;
        raster.polygonMode = vk::PolygonMode::eFill;
        raster.lineWidth = 1.0f;

        // Dynamic states (like original)
        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport, vk::DynamicState::eScissor,
            vk::DynamicState::eCullMode, vk::DynamicState::eFrontFace,
            vk::DynamicState::ePrimitiveTopology};

        vk::PipelineDynamicStateCreateInfo dynamicState;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Color blending (like original)
        vk::PipelineColorBlendAttachmentState blendAttachment;
        blendAttachment.colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo blend;
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        // Viewport (like original)
        vk::PipelineViewportStateCreateInfo viewport;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        // Depth/stencil (like original)
        vk::PipelineDepthStencilStateCreateInfo depthStencil;
        depthStencil.depthCompareOp = vk::CompareOp::eAlways;

        // Multisample (like original)
        vk::PipelineMultisampleStateCreateInfo multisample;
        multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

        // Shader stages (using embedded SPIR-V or file loading)
        vk::ShaderModule shaderModule =
            createShaderModule(readFile("shaders/09_shader_base.spv"));

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = "vertMain"};
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = "fragMain"};
        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
            vertShaderStageInfo, fragShaderStageInfo};

        // Dynamic rendering (like original)
        vk::PipelineRenderingCreateInfo renderingInfo;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachmentFormats = &context.swapchain_dimensions.format;

        // Create pipeline (like original)
        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.pNext = &renderingInfo;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = context.pipeline_layout;
        pipelineInfo.renderPass = nullptr;

        auto result = context.device.createGraphicsPipeline(nullptr, pipelineInfo);
        if (result.result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
        context.pipeline = result.value;

        // Cleanup shader modules (like original)
        context.device.destroyShaderModule(shaderModule);
    }

    std::vector<char> readFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    vk::ShaderModule createShaderModule(const std::vector<char> &code)
    {
        vk::ShaderModuleCreateInfo createInfo;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
        return context.device.createShaderModule(createInfo);
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            update(0.0f); // delta_time not used in this simple example
        }

        context.device.waitIdle();
    }

    void update(float /*delta_time*/)
    {
        uint32_t imageIndex;
        auto res = acquireNextSwapchainImage(&imageIndex);

        if (res == vk::Result::eSuboptimalKHR || res == vk::Result::eErrorOutOfDateKHR ||
            framebufferResized)
        {
            framebufferResized = false;
            recreateSwapchain();
            res = acquireNextSwapchainImage(&imageIndex);
        }

        if (res != vk::Result::eSuccess)
        {
            context.queue.waitIdle();
            return;
        }

        renderTriangle(imageIndex);
        res = presentImage(imageIndex);

        if (res == vk::Result::eSuboptimalKHR || res == vk::Result::eErrorOutOfDateKHR ||
            framebufferResized)
        {
            framebufferResized = false;
            recreateSwapchain();
        }
        else if (res != vk::Result::eSuccess)
        {
            LOGE("Failed to present swapchain image.");
        }
    }

    vk::Result acquireNextSwapchainImage(uint32_t *image)
    {
        vk::Semaphore acquireSemaphore;
        if (context.recycled_semaphores.empty())
        {
            acquireSemaphore = context.device.createSemaphore({});
        }
        else
        {
            acquireSemaphore = context.recycled_semaphores.back();
            context.recycled_semaphores.pop_back();
        }

        vk::Result result;
        try
        {
            auto acquireResult = context.device.acquireNextImageKHR(
                context.swapchain, UINT64_MAX, acquireSemaphore);
            result = acquireResult.result;
            *image = acquireResult.value;
        }
        catch (vk::OutOfDateKHRError &)
        {
            result = vk::Result::eErrorOutOfDateKHR;
        }

        if (result != vk::Result::eSuccess)
        {
            context.recycled_semaphores.push_back(acquireSemaphore);
        }
        else
        {
            // Wait for fence (like original)
            if (context.per_frame[*image].queue_submit_fence)
            {
                context.device.waitForFences(context.per_frame[*image].queue_submit_fence,
                                             true, UINT64_MAX);
                context.device.resetFences(context.per_frame[*image].queue_submit_fence);
            }

            // Reset command pool (like original)
            if (context.per_frame[*image].primary_command_pool)
            {
                context.device.resetCommandPool(
                    context.per_frame[*image].primary_command_pool);
            }

            // Recycle old semaphore (like original)
            vk::Semaphore oldSemaphore =
                context.per_frame[*image].swapchain_acquire_semaphore;
            if (oldSemaphore)
            {
                context.recycled_semaphores.push_back(oldSemaphore);
            }

            context.per_frame[*image].swapchain_acquire_semaphore = acquireSemaphore;
        }

        return result;
    }

    void renderTriangle(uint32_t swapchain_index)
    {
        vk::CommandBuffer cmd = context.per_frame[swapchain_index].primary_command_buffer;

        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd.begin(beginInfo);

        // Transition layout (like original)
        transitionImageLayout(cmd, context.swapchain_images[swapchain_index],
                              vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eColorAttachmentOptimal, {},
                              vk::AccessFlagBits2::eColorAttachmentWrite,
                              vk::PipelineStageFlagBits2::eTopOfPipe,
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput);

        // Clear color (like original)
        vk::ClearValue clearValue;
        clearValue.color = std::array<float, 4>{0.01f, 0.01f, 0.033f, 1.0f};

        // Begin rendering (like original)
        vk::RenderingAttachmentInfo colorAttachment;
        colorAttachment.imageView = context.swapchain_image_views[swapchain_index];
        colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.clearValue = clearValue;

        vk::RenderingInfo renderingInfo;
        renderingInfo.renderArea =
            vk::Rect2D({0, 0}, {context.swapchain_dimensions.width,
                                context.swapchain_dimensions.height});
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        cmd.beginRendering(renderingInfo);

        // Bind pipeline (like original)
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, context.pipeline);

        // Set dynamic states (like original)
        vk::Viewport viewport;
        viewport.width = static_cast<float>(context.swapchain_dimensions.width);
        viewport.height = static_cast<float>(context.swapchain_dimensions.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmd.setViewport(0, viewport);

        vk::Rect2D scissor;
        scissor.extent = {context.swapchain_dimensions.width,
                          context.swapchain_dimensions.height};
        cmd.setScissor(0, scissor);

        cmd.setCullMode(vk::CullModeFlagBits::eNone);
        cmd.setFrontFace(vk::FrontFace::eClockwise);
        cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

        // Bind vertex buffer (like original)
        cmd.bindVertexBuffers(0, context.vertex_buffer, {0});

        // Draw (like original)
        cmd.draw(vertices.size(), 1, 0, 0);

        cmd.endRendering();

        // Transition for present (like original)
        transitionImageLayout(cmd, context.swapchain_images[swapchain_index],
                              vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::ePresentSrcKHR,
                              vk::AccessFlagBits2::eColorAttachmentWrite, {},
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                              vk::PipelineStageFlagBits2::eBottomOfPipe);

        cmd.end();

        // Submit (like original)
        if (!context.per_frame[swapchain_index].swapchain_release_semaphore)
        {
            context.per_frame[swapchain_index].swapchain_release_semaphore =
                context.device.createSemaphore({});
        }

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eTopOfPipe;

        vk::SubmitInfo submitInfo;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores =
            &context.per_frame[swapchain_index].swapchain_acquire_semaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores =
            &context.per_frame[swapchain_index].swapchain_release_semaphore;

        context.queue.submit(submitInfo,
                             context.per_frame[swapchain_index].queue_submit_fence);
    }

    void transitionImageLayout(vk::CommandBuffer cmd, vk::Image image,
                               vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                               vk::AccessFlags2 srcAccessMask,
                               vk::AccessFlags2 dstAccessMask,
                               vk::PipelineStageFlags2 srcStage,
                               vk::PipelineStageFlags2 dstStage)
    {
        vk::ImageMemoryBarrier2 barrier;
        barrier.srcStageMask = srcStage;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstStageMask = dstStage;
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;

        cmd.pipelineBarrier2(dependencyInfo);
    }

    vk::Result presentImage(uint32_t index)
    {
        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores =
            &context.per_frame[index].swapchain_release_semaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &context.swapchain;
        presentInfo.pImageIndices = &index;

        try
        {
            return context.queue.presentKHR(presentInfo);
        }
        catch (vk::OutOfDateKHRError &)
        {
            return vk::Result::eErrorOutOfDateKHR;
        }
    }

    void recreateSwapchain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        context.device.waitIdle();
        cleanupSwapchain();
        initSwapchain();
    }

    void cleanupSwapchain()
    {
        for (auto imageView : context.swapchain_image_views)
        {
            context.device.destroyImageView(imageView);
        }
        context.swapchain_image_views.clear();

        if (context.swapchain)
        {
            context.device.destroySwapchainKHR(context.swapchain);
        }

        for (auto &per_frame : context.per_frame)
        {
            teardownPerFrame(per_frame);
        }
        context.per_frame.clear();
    }

    void teardownPerFrame(PerFrame &per_frame)
    {
        if (per_frame.queue_submit_fence)
        {
            context.device.destroyFence(per_frame.queue_submit_fence);
        }

        if (per_frame.primary_command_buffer)
        {
            context.device.freeCommandBuffers(per_frame.primary_command_pool,
                                              per_frame.primary_command_buffer);
        }

        if (per_frame.primary_command_pool)
        {
            context.device.destroyCommandPool(per_frame.primary_command_pool);
        }

        if (per_frame.swapchain_acquire_semaphore)
        {
            context.device.destroySemaphore(per_frame.swapchain_acquire_semaphore);
        }

        if (per_frame.swapchain_release_semaphore)
        {
            context.device.destroySemaphore(per_frame.swapchain_release_semaphore);
        }
    }

    bool validateExtensions(const std::vector<const char *> &required,
                            const std::vector<vk::ExtensionProperties> &available)
    {
        return std::ranges::all_of(required, [&available](const char *ext) {
            bool found = std::ranges::any_of(available, [ext](const auto &avail) {
                return strcmp(avail.extensionName, ext) == 0;
            });
            if (!found)
            {
                LOGE("Required extension not found: {}", ext);
            }
            return found;
        });
    }

    void cleanup()
    {
        if (context.device)
        {
            context.device.waitIdle();
        }

        cleanupSwapchain();

        for (auto &per_frame : context.per_frame)
        {
            teardownPerFrame(per_frame);
        }
        context.per_frame.clear();

        for (auto semaphore : context.recycled_semaphores)
        {
            context.device.destroySemaphore(semaphore);
        }

        if (context.pipeline)
        {
            context.device.destroyPipeline(context.pipeline);
        }

        if (context.pipeline_layout)
        {
            context.device.destroyPipelineLayout(context.pipeline_layout);
        }

        for (auto imageView : context.swapchain_image_views)
        {
            context.device.destroyImageView(imageView);
        }

        if (context.swapchain)
        {
            context.device.destroySwapchainKHR(context.swapchain);
        }

        if (context.surface)
        {
            context.instance.destroySurfaceKHR(context.surface);
        }

        if (context.vertex_buffer)
        {
            context.device.destroyBuffer(context.vertex_buffer);
        }

        if (context.vertex_buffer_memory)
        {
            context.device.freeMemory(context.vertex_buffer_memory);
        }

        if (context.device)
        {
            context.device.destroy();
        }

        if (context.debug_callback)
        {
            context.instance.destroyDebugUtilsMessengerEXT(context.debug_callback);
        }

        if (context.instance)
        {
            context.instance.destroy();
        }

        if (window)
        {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    }
};

int main()
{
    HPPHelloTriangleV13 app;

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