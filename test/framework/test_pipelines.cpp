#include "./head.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <exception>
#include <iostream>
#include <print>
#include <utility>
#include <chrono>

#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

using surface = mcs::vulkan::wsi::glfw::Window;

using make_physical_device = mcs::vulkan::make_physical_device;
using make_instance = mcs::vulkan::make_instance;
using physical_device = mcs::vulkan::physical_device;
using make_logical_device = mcs::vulkan::make_logical_device;
using make_queue_family_index = mcs::vulkan::make_queue_family_index;

using logical_device = mcs::vulkan::logical_device;

using mcs::vulkan::structure_chain;
using mcs::vulkan::sType;

using mcs::vulkan::choose_swap_surface_format;
using mcs::vulkan::choose_swap_present_mode;

struct frame_context
{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    const logical_device *device_{};
    std::vector<VkSemaphore> presentCompleteSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;

    explicit frame_context(const logical_device &device, size_t swapChainImagesSize)
        : device_{&device}
    {
        createSyncObjects(device.raw_data(), swapChainImagesSize);
    }
    ~frame_context() noexcept
    {
        destroy();
    }
    frame_context(const frame_context &) = delete;
    frame_context(frame_context &&) = delete;
    frame_context &operator=(const frame_context &) = delete;
    frame_context &operator=(frame_context &&) = delete;

  private:
    void createSyncObjects(const VkDevice &device, size_t swapChainImagesSize)
    {
        destroySyncObject();

        presentCompleteSemaphore.resize(swapChainImagesSize);
        renderFinishedSemaphore.resize(swapChainImagesSize);
        VkSemaphoreCreateInfo semaphoreInfo = {.sType = sType<VkSemaphoreCreateInfo>()};

        for (size_t i = 0; i < swapChainImagesSize; i++)
        {
            if (::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                    &presentCompleteSemaphore[i]) != VK_SUCCESS ||
                ::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                    &renderFinishedSemaphore[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphores!");
        }

        VkFenceCreateInfo fenceInfo = {.sType = sType<VkFenceCreateInfo>(),
                                       .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (::vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create fence!");
        }
    }

    constexpr void destroySyncObject() noexcept
    {
        for (auto *semaphore : presentCompleteSemaphore)
            ::vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
        presentCompleteSemaphore.clear();

        for (auto *semaphore : renderFinishedSemaphore)
            ::vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
        renderFinishedSemaphore.clear();

        for (auto *fence : inFlightFences)
            ::vkDestroyFence(device_->raw_data(), fence, nullptr);
    }

    constexpr void destroy() noexcept
    {
        if (device_ != nullptr)
        {
            destroySyncObject();
            device_ = nullptr;
        }
    }
};

struct my_render
{
    static void transition_image_layout(VkCommandBuffer commandBuffer, VkImage image,
                                        VkImageAspectFlags aspectMask,
                                        VkImageLayout oldLayout, VkImageLayout newLayout,
                                        VkAccessFlags srcAccessMask,
                                        VkAccessFlags dstAccessMask,
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask)
    {
        VkImageMemoryBarrier2 barrier = {.sType = sType<VkImageMemoryBarrier2>(),
                                         .srcStageMask = srcStageMask,
                                         .srcAccessMask = srcAccessMask,
                                         .dstStageMask = dstStageMask,
                                         .dstAccessMask = dstAccessMask,
                                         .oldLayout = oldLayout,
                                         .newLayout = newLayout,
                                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .image = image,
                                         .subresourceRange = {.aspectMask = aspectMask,
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1}};
        VkDependencyInfo dependency_info = {.sType = sType<VkDependencyInfo>(),
                                            .dependencyFlags = {},
                                            .imageMemoryBarrierCount = 1,
                                            .pImageMemoryBarriers = &barrier};
        ::vkCmdPipelineBarrier2(commandBuffer, &dependency_info);
    }
};

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "Wireframe & Solid Rendering";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

enum class RenderMode
{
    WIREFRAME_ONLY,
    SOLID_AND_WIREFRAME,
    SOLID_ONLY
};

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

struct GraphicsPipelines
{
    VkPipeline solid = nullptr;
    VkPipeline wireframe = nullptr;
    VkPipelineLayout layout = nullptr;
};

GraphicsPipelines createGraphicsPipelines(const logical_device &device,
                                          VkFormat swapChainImageFormat)
{
    GraphicsPipelines pipelines;

    // 创建着色器模块
    mcs::vulkan::shader_module vertshader{device, "shaders/test_pipelines_vert.spv"};
    mcs::vulkan::shader_module fragshader{device, "shaders/test_pipelines_frag.spv"};

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = sType<VkPipelineShaderStageCreateInfo>(),
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertshader.raw_data(),
        .pName = "main"};

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = sType<VkPipelineShaderStageCreateInfo>(),
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragshader.raw_data(),
        .pName = "main"};

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = sType<VkPipelineInputAssemblyStateCreateInfo>(),
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE};

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = sType<VkPipelineViewportStateCreateInfo>(),
        .viewportCount = 1,
        .scissorCount = 1};

    // 实体渲染的光栅化状态
    VkPipelineRasterizationStateCreateInfo rasterizerSolid = {
        .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0F};

    // 线框渲染的光栅化状态
    VkPipelineRasterizationStateCreateInfo rasterizerWireframe = {
        .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_LINE,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_TRUE, // 启用深度偏移避免深度冲突
        .depthBiasConstantFactor = 1.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0F};

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = sType<VkPipelineColorBlendStateCreateInfo>(),
        .logicOpEnable = VK_FALSE,
        .logicOp = VkLogicOp::VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH};

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = sType<VkPipelineDynamicStateCreateInfo>(),
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = sType<VkPipelineLayoutCreateInfo>(), .setLayoutCount = 0};

    try
    {
        // 创建管线布局
        pipelines.layout = device.createPipelineLayout(pipelineLayoutInfo, nullptr);

        // 使用动态渲染
        structure_chain<VkGraphicsPipelineCreateInfo, VkPipelineRenderingCreateInfo>
            pipelineCreateInfoChain = {
                {.stageCount = 2,
                 .pStages = shaderStages,
                 .pVertexInputState = &vertexInputInfo,
                 .pInputAssemblyState = &inputAssembly,
                 .pViewportState = &viewportState,
                 .pMultisampleState = &multisampling,
                 .pColorBlendState = &colorBlending,
                 .pDynamicState = &dynamicState,
                 .layout = pipelines.layout,
                 .renderPass = VK_NULL_HANDLE},
                {.colorAttachmentCount = 1,
                 .pColorAttachmentFormats = &swapChainImageFormat}};

        // 先创建基础管线（实体填充）
        auto pipelineSolidInfo = pipelineCreateInfoChain;
        pipelineSolidInfo.head().pRasterizationState = &rasterizerSolid;
        pipelineSolidInfo.head().flags =
            VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT; // 允许派生

        pipelines.solid =
            device.createGraphicsPipelines(nullptr, 1, pipelineSolidInfo.head(), nullptr);

        // 然后基于实体管线创建线框管线（使用basePipelineHandle）
        auto pipelineWireframeInfo = pipelineCreateInfoChain;
        pipelineWireframeInfo.head().pRasterizationState = &rasterizerWireframe;
        pipelineWireframeInfo.head().flags =
            VK_PIPELINE_CREATE_DERIVATIVE_BIT; // 这是派生管线
        pipelineWireframeInfo.head().basePipelineHandle =
            pipelines.solid;                                 // 关键：设置基础管线
        pipelineWireframeInfo.head().basePipelineIndex = -1; // 使用handle时index必须为-1

        pipelines.wireframe = device.createGraphicsPipelines(
            nullptr, 1, pipelineWireframeInfo.head(), nullptr);

        return pipelines;
    }
    catch (...)
    {
        // 清理资源
        if (pipelines.wireframe != nullptr)
            device.destroyPipeline(pipelines.wireframe, nullptr);
        if (pipelines.solid != nullptr)
            device.destroyPipeline(pipelines.solid, nullptr);
        if (pipelines.layout != nullptr)
            device.destroyPipelineLayout(pipelines.layout, nullptr);
        throw;
    }
}

int main()
{
    try
    {
        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

        // 启用所需特性
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = sType<VkPhysicalDeviceFeatures2>();
        features2.features.fillModeNonSolid = VK_TRUE; // 启用线框模式
        features2.features.wideLines = VK_TRUE;        // 启用宽线支持

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                std::move(features2),
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

        // 当前渲染模式
        RenderMode currentRenderMode = RenderMode::SOLID_AND_WIREFRAME;

        // 创建窗口
        surface window{};
        window.setup({.width = WIDTH, .height = HEIGHT}, TITLE);

        // 设置键盘回调切换渲染模式
        currentRenderMode = RenderMode::SOLID_AND_WIREFRAME;

        // 创建实例
        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "Wireframe & Solid Rendering",
                                    .applicationVersion = VkApiVersion(1, 0, 0),
                                    .pEngineName = "No Engine",
                                    .engineVersion = VkApiVersion(1, 0, 0),
                                    .apiVersion = VkApiVersion(0, 1, 4, 0)});

        // 创建表面
        VkSurfaceKHR surface_ = window.createVkSurfaceKHR(instance.ref_data());
        assert(surface_ != nullptr);

        // 选择物理设备
        auto physicalDevice =
            make_physical_device{instance.ref_data()}
                .requiredDeviceProperties([](const VkPhysicalDeviceProperties
                                                 &device_properties) constexpr noexcept {
                    return device_properties.apiVersion >= VK_API_VERSION_1_3;
                })
                .requiredQueueFamilyProperties(
                    [](const VkQueueFamilyProperties &qfp) constexpr noexcept {
                        return !!(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT);
                    })
                .requiredDeviceExtensions(requiredDeviceExtension)
                .requiredFeatures(
                    [](const physical_device &physicalDevice) constexpr noexcept -> bool {
                        auto query = structure_chain<
                            VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{{}, {}, {}};
                        physicalDevice.getFeatures2(query.head());
                        auto &query_vulkan13_features =
                            query.template get<VkPhysicalDeviceVulkan13Features>();
                        auto &query_extended_dynamic_state_features = query.template get<
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                        auto &query_features =
                            query.template get<VkPhysicalDeviceFeatures2>();

                        return query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
                               query_extended_dynamic_state_features
                                   .extendedDynamicState &&
                               query_features.features.fillModeNonSolid &&
                               query_features.features.wideLines;
                    })
                .pickPhysicalDevice();

        // 获取图形队列族索引
        auto graphicsIndex =
            make_queue_family_index{physicalDevice}
                .requiredQueueFamilyProperties(
                    [&](const VkQueueFamilyProperties &qfp, uint32_t queueFamilyIndex,
                        const physical_device &physicalDevice) noexcept -> bool {
                        return (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                               physicalDevice.getSurfaceSupportKHR(queueFamilyIndex,
                                                                   surface_);
                    })
                .build();

        // 创建逻辑设备
        auto logical_device_ = [&]() {
            float queuePriority = 0.0F;
            VkDeviceQueueCreateInfo deviceQueueCreateInfo{
                .sType = sType<VkDeviceQueueCreateInfo>(),
                .queueFamilyIndex = graphicsIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority};
            VkDeviceCreateInfo deviceCreateInfo{
                .sType = sType<VkDeviceCreateInfo>(),
                .pNext = &enablefeatureChain.head(),
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &deviceQueueCreateInfo,
                .enabledExtensionCount =
                    static_cast<uint32_t>(requiredDeviceExtension.size()),
                .ppEnabledExtensionNames = requiredDeviceExtension.data(),
            };
            return logical_device{
                physicalDevice.createDevice(&deviceCreateInfo, nullptr)};
        }();

        // 获取图形队列
        auto *graphicsQueue = logical_device_.getDeviceQueue(graphicsIndex, 0);

        // 获取呈现队列
        auto presentIndex = [&]() {
            std::vector<VkQueueFamilyProperties> queueFamilyProperties =
                physicalDevice.getQueueFamilyProperties();
            auto presentIndex =
                physicalDevice.getSurfaceSupportKHR(graphicsIndex, surface_)
                    ? graphicsIndex
                    : ~0;
            if (presentIndex == queueFamilyProperties.size())
            {
                for (size_t i = 0; i < queueFamilyProperties.size(); i++)
                {
                    if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                        physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i),
                                                            surface_))
                    {
                        graphicsIndex = static_cast<uint32_t>(i);
                        presentIndex = graphicsIndex;
                        break;
                    }
                }
                if (presentIndex == queueFamilyProperties.size())
                {
                    for (size_t i = 0; i < queueFamilyProperties.size(); i++)
                    {
                        if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i),
                                                                surface_))
                        {
                            presentIndex = static_cast<uint32_t>(i);
                            break;
                        }
                    }
                }
            }
            if (presentIndex == ~0)
            {
                throw std::runtime_error(
                    "Could not find a queue for present -> terminating");
            }
            return presentIndex;
        }();
        auto *presentQueue = logical_device_.getDeviceQueue(presentIndex, 0);

        // 创建交换链
        VkFormat swapChainImageFormat{};
        VkExtent2D swapChainExtent;
        VkSwapchainKHR swapChain{};
        std::vector<VkImage> swapChainImages{};

        auto createSwapChain = [&]() {
            auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface_);
            auto swapChainFormat = choose_swap_surface_format(
                VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_SRGB,
                                   .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                physicalDevice.getSurfaceFormatsKHR(surface_));
            swapChainImageFormat = swapChainFormat.format;

            swapChainExtent = window.chooseSwapExtent(surfaceCapabilities);

            auto minImageCount = std::max(3U, surfaceCapabilities.minImageCount);
            minImageCount = (surfaceCapabilities.maxImageCount > 0 &&
                             minImageCount > surfaceCapabilities.maxImageCount)
                                ? surfaceCapabilities.maxImageCount
                                : minImageCount;

            VkSwapchainCreateInfoKHR swapChainCreateInfo{
                .sType = sType<VkSwapchainCreateInfoKHR>(),
                .surface = surface_,
                .minImageCount = minImageCount,
                .imageFormat = swapChainImageFormat,
                .imageColorSpace = swapChainFormat.colorSpace,
                .imageExtent = swapChainExtent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .preTransform = surfaceCapabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = choose_swap_present_mode(
                    VK_PRESENT_MODE_MAILBOX_KHR,
                    physicalDevice.getSurfacePresentModesKHR(surface_)),
                .clipped = true};

            swapChain = logical_device_.createSwapchainKHR(swapChainCreateInfo);
            swapChainImages = logical_device_.getSwapchainImagesKHR(swapChain);
        };
        createSwapChain();

        // 创建图像视图
        std::vector<VkImageView> swapChainImageViews;
        auto createImageViews = [&]() {
            swapChainImageViews.clear();
            VkImageViewCreateInfo imageViewCreateInfo{
                .sType = sType<VkImageViewCreateInfo>(),
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapChainImageFormat,
                .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            for (auto *image : swapChainImages)
            {
                imageViewCreateInfo.image = image;
                swapChainImageViews.emplace_back(
                    logical_device_.createImageView(imageViewCreateInfo, nullptr));
            }
        };
        createImageViews();

        // 创建两个管线（使用basePipelineHandle）
        GraphicsPipelines pipelines =
            createGraphicsPipelines(logical_device_, swapChainImageFormat);

        // 创建命令池
        auto *commandPool = logical_device_.createCommandPool(
            {.sType = sType<VkCommandPoolCreateInfo>(),
             .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
             .queueFamilyIndex = graphicsIndex});

        // 创建命令缓冲区
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
        logical_device_.allocateCommandBuffers(
            commandBuffers[0],
            {.sType = sType<VkCommandBufferAllocateInfo>(),
             .commandPool = commandPool,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())});

        // 创建帧上下文
        frame_context frameContext{logical_device_, swapChainImages.size()};

        // 清理和重建交换链
        auto cleanupSwapChain = [&]() {
            for (auto *imageView : swapChainImageViews)
                logical_device_.destroyImageView(imageView, nullptr);
            if (swapChain != nullptr)
                logical_device_.destroySwapchainKHR(swapChain);

            swapChainImageViews.clear();
            swapChain = nullptr;
        };

        auto recreateSwapChain = [&]() {
            window.waitGoodFramebufferSize();
            logical_device_.waitIdle();

            cleanupSwapChain();
            createSwapChain();
            createImageViews();
        };

        // 记录命令缓冲区
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];
            if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // 转换交换链图像布局
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.1F, 0.1F, 0.1F, 1.0F}}};

            VkRenderingAttachmentInfo colorAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = imageView,
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = clearColor};

            VkRenderingInfo renderingInfo = {
                .sType = sType<VkRenderingInfo>(),
                .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment,
                .pDepthAttachment = nullptr};

            ::vkCmdBeginRendering(commandBuffer, &renderingInfo);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // 根据当前模式绘制
            constexpr auto COUNT = 36;
            switch (currentRenderMode)
            {
            case RenderMode::WIREFRAME_ONLY:
                // 仅显示网格
                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelines.wireframe);
                ::vkCmdSetLineWidth(commandBuffer, 2.0f);
                ::vkCmdDraw(commandBuffer, COUNT, 1, 0, 0);
                break;

            case RenderMode::SOLID_AND_WIREFRAME:
                // 显示实体+网格（先实体后线框）
                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelines.solid);
                ::vkCmdDraw(commandBuffer, COUNT, 1, 0, 0);

                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelines.wireframe);
                ::vkCmdSetLineWidth(commandBuffer, 2.0f);
                ::vkCmdDraw(commandBuffer, COUNT, 1, 0, 0);
                break;

            case RenderMode::SOLID_ONLY:
                // 仅显示实体
                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelines.solid);
                ::vkCmdDraw(commandBuffer, COUNT, 1, 0, 0);
                break;
            }

            ::vkCmdEndRendering(commandBuffer);

            // 转换交换链图像布局到呈现源
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (::vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        };

        // 绘制帧
        auto drawFrame = [&]() {
            auto &inFlightFences = frameContext.inFlightFences;
            auto &currentFrame = frameContext.currentFrame;
            auto &presentCompleteSemaphore = frameContext.presentCompleteSemaphore;
            auto &semaphoreIndex = frameContext.semaphoreIndex;
            auto &renderFinishedSemaphore = frameContext.renderFinishedSemaphore;

            auto *device = logical_device_.raw_data();
            while (::vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                                     UINT64_MAX) == VK_TIMEOUT)
                ;
            uint32_t imageIndex;
            VkResult result = ::vkAcquireNextImageKHR(
                device, swapChain, UINT64_MAX, presentCompleteSemaphore[semaphoreIndex],
                VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreateSwapChain();
                return;
            }
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            ::vkResetFences(device, 1, &inFlightFences[currentFrame]);

            auto *commandBuffer = commandBuffers[currentFrame];

            ::vkResetCommandBuffer(commandBuffer, 0);
            recordCommandBuffer(commandBuffer, imageIndex);

            VkPipelineStageFlags waitDestinationStageMask[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

            VkSubmitInfo submitInfo = {
                .sType = sType<VkSubmitInfo>(),
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &presentCompleteSemaphore[semaphoreIndex],
                .pWaitDstStageMask = waitDestinationStageMask,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinishedSemaphore[imageIndex]};

            if (::vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                                inFlightFences[currentFrame]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            VkPresentInfoKHR presentInfo = {.sType = sType<VkPresentInfoKHR>(),
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores =
                                                &renderFinishedSemaphore[imageIndex],
                                            .swapchainCount = 1,
                                            .pSwapchains = &swapChain,
                                            .pImageIndices = &imageIndex};

            result = ::vkQueuePresentKHR(presentQueue, &presentInfo);
            if (auto &framebufferResized = window.refFramebufferResized();
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                framebufferResized)
            {
                framebufferResized = false;
                recreateSwapChain();
            }

            semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        };

        // 主循环
        while (window.shouldClose() == 0)
        {
            surface::pollEvents();

            // diff:   每一秒切换
            auto now = std::chrono::steady_clock::now();
            static auto lastUpdate = std::chrono::steady_clock::now();
            if (now - lastUpdate > std::chrono::seconds(1))
            {
                int v = static_cast<int>(currentRenderMode);
                currentRenderMode = static_cast<RenderMode>((v + 1) % 3);

                lastUpdate = now;
            }

            drawFrame();
        }

        // 等待设备空闲
        ::vkDeviceWaitIdle(logical_device_.raw_data());

        // 清理资源
        if (pipelines.wireframe != nullptr)
            logical_device_.destroyPipeline(pipelines.wireframe, nullptr);
        if (pipelines.solid != nullptr)
            logical_device_.destroyPipeline(pipelines.solid, nullptr);
        if (pipelines.layout != nullptr)
            logical_device_.destroyPipelineLayout(pipelines.layout, nullptr);

        cleanupSwapChain();
        logical_device_.destroyCommandPool(commandPool);
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface_);
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "main done\n";
    return 0;
}