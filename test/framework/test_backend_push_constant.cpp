#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <print>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "backend/Queue.hpp"
#include "backend/app_context.hpp"
#include "backend/create_graphics_pipeline.hpp"
#include "backend/create_instance.hpp"
#include "backend/create_pipeline_layout.hpp"
#include "backend/frame_context.hpp"
#include "backend/surface_impl.hpp"
#include "backend/swap_chain_interface.hpp"
#include "backend/wsi/glfw.hpp"
#include "backend/pick_physical_device.hpp"
#include "backend/create_logical_device.hpp"
#include "backend/sType.hpp"
#include "backend/select_queue_family_index.hpp"
#include "backend/structure_chain.hpp"
#include "backend/CommandPool.hpp"

using app_context = mcs::vulkan::core::app_context;
using create_instance = mcs::vulkan::core::create_instance;

using surface = mcs::vulkan::wsi::glfw::Window;
using pick_physical_device = mcs::vulkan::core::pick_physical_device;
using mcs::vulkan::core::structure_chain;

using PhysicalDevice = mcs::vulkan::core::PhysicalDevice;
using select_queue_family_index = mcs::vulkan::core::select_queue_family_index;
using create_logical_device = mcs::vulkan::core::create_logical_device;
using LogicalDevice = mcs::vulkan::core::LogicalDevice;
using swap_chain_interface = mcs::vulkan::core::swap_chain_interface;
using resources_interface = mcs::vulkan::core::resources_interface;
using surface_interface = mcs::vulkan::core::surface_interface;
using mcs::vulkan::core::surface_impl;

using create_pipeline_layout = mcs::vulkan::core::create_pipeline_layout;
using create_graphics_pipeline = mcs::vulkan::core::create_graphics_pipeline;

using CommandPool = mcs::vulkan::core::CommandPool;
using CommandBuffer = mcs::vulkan::core::CommandBuffer;
using Queue = mcs::vulkan::core::Queue;

using mcs::vulkan::core::sType;
using mcs::vulkan::core::vkApiVersion;
using mcs::vulkan::core::vkMakeVersion;
using mcs::vulkan::core::frame_context;

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "test_my_triangle";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct my_render
{
    // NOLINTNEXTLINE
    constexpr static void transition_image_layout(
        const CommandBuffer &commandBuffer, VkImage image, VkImageAspectFlags aspectMask,
        VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask, // NOLINT
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask // NOLINT
        ) noexcept
    {
        VkImageMemoryBarrier2 barrier = {
            .sType = sType<VkImageMemoryBarrier2>(),
            // Specify the pipeline stages and access masks for the barrier
            .srcStageMask = srcStageMask,
            .srcAccessMask = srcAccessMask,
            .dstStageMask = dstStageMask,
            .dstAccessMask = dstAccessMask,
            // Specify the old and new layouts of the image
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            // We are not changing the ownership between queues
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            // Specify the image to be affected by this barrier
            .image = image,
            // Define the subresource range (which parts of the image are affected)
            .subresourceRange = {.aspectMask = aspectMask,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};
        VkDependencyInfo dependency_info = {.sType = sType<VkDependencyInfo>(),
                                            .dependencyFlags = {},
                                            .imageMemoryBarrierCount = 1,
                                            .pImageMemoryBarriers = &barrier};
        commandBuffer.pipelineBarrier2(dependency_info);
    }
};

// diff: start
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

//  推送常量结构体
struct alignas(16) PushConstants
{
    glm::vec4 positions[3];
    glm::vec4 colors[3];
};
static_assert(sizeof(PushConstants) % 16 == 0, "PushConstants must be 16-byte aligned");

constexpr auto VERT_SHADER_PATH = "shaders/test_backend_push_constant_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_triangle_frag.spv";

// diff: end

// NOTE: 绘制一个三角形 . NOTE:至少是3角形绘制,绘制4个顶点是错误的.除非修改拓扑
int main()
try
{
    surface window{};
    window.setup({.width = WIDTH, .height = HEIGHT}, TITLE); // NOLINT

    // NOTE: 扩展功能 和 你使用的 功能是相关的. 比如 SPIRV 着色器扩展.物理设备必须支持
    std::vector<const char *> requiredDeviceExtension = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}; // NOLINTEND

    structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
        enablefeatureChain = {{},
                              {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                              {.extendedDynamicState = VK_TRUE}};

    app_context ctx{};
    ctx.setInstance(
           create_instance{ctx}
               .enableDebug()
               .enableSurface<surface>()
               .setApplicationInfo({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "Hello Triangle",
                                    .applicationVersion = vkMakeVersion(1, 0, 0),
                                    .pEngineName = "No Engine",
                                    .engineVersion = vkMakeVersion(1, 0, 0),
                                    // apiVersion必须是应用程序设计使用的Vulkan的最高版本
                                    .apiVersion = vkApiVersion(0, 1, 4, 0)})
               .create())
        .setDebug()
        .setSurface(window.createVkSurfaceKHR(*ctx.instance()))
        .setPhysicalDevice(
            pick_physical_device{ctx}
                .requiredProperties([](const VkPhysicalDeviceProperties
                                           &device_properties) constexpr noexcept {
                    return device_properties.apiVersion >= VK_API_VERSION_1_3;
                })
                .requiredQueueFamily(
                    [](const VkQueueFamilyProperties &qfp) constexpr noexcept {
                        return !!(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT);
                    })
                .requiredDeviceExtension(requiredDeviceExtension)
                .requiredFeatures(
                    [](const PhysicalDevice &physicalDevice) constexpr noexcept -> bool {
                        auto query = structure_chain<
                            VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{{}, {}, {}};
                        physicalDevice.getFeatures2(&query.head());

                        auto &query_vulkan13_features =
                            query.template get<VkPhysicalDeviceVulkan13Features>();
                        auto &query_extended_dynamic_state_features = query.template get<
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                        return query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
                               query_extended_dynamic_state_features.extendedDynamicState;
                    })
                .check()
                .pickIndex([](const std::vector<PhysicalDevice *> &candidate) noexcept
                               -> size_t {
                    assert(candidate.size() > 0);
                    return 0;
                })
                .create());
    // NOTE: Queue Family 中 选一个 queue 来和 物理设备交互?
    const auto GRAPHICS_QUEUE_FAMILY_IDX =
        select_queue_family_index{ctx.physicalDevice()}
            .requiredQueueFamily([&](const VkQueueFamilyProperties &qfp,
                                     uint32_t queueFamilyIndex) -> bool {
                return (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                       ctx.physicalDevice().getSurfaceSupportKHR(queueFamilyIndex,
                                                                 ctx.surface());
            })
            .check()
            .select([](const std::vector<uint32_t> &candidate) noexcept -> uint32_t {
                assert(candidate.size() > 0);
                std::println("candidate graphics&surface family: {}", candidate.size());
                return 0;
            })
            .create();

    float queuePriority = 0.0F;
    Queue graphicsAndPresentQueue;
    ctx.setLogicalDevice(
        create_logical_device{ctx, ctx.physicalDevice()}
            .setFlags({})
            .setQueueCreateInfos({{.sType = sType<VkDeviceQueueCreateInfo>(),
                                   .queueFamilyIndex = GRAPHICS_QUEUE_FAMILY_IDX,
                                   .queueCount = 1,
                                   .pQueuePriorities = &queuePriority}})
            .setEnableFeatureChain(&enablefeatureChain.head())
            .setEnabledExtension(requiredDeviceExtension)
            .create([&](LogicalDevice &device) noexcept {
                graphicsAndPresentQueue =
                    Queue{device, device.getDeviceQueue(GRAPHICS_QUEUE_FAMILY_IDX, 0)};
            }));
    requiredDeviceExtension = {}; // NOTE: no need more

    auto swapChain =
        swap_chain_interface{ctx.logicalDevice(), std::make_unique<surface_impl<surface>>(
                                                      window, ctx.surface())}
            .configSwapchain(
                {.flags = {},
                 .minImageCountStrategy =
                     swap_chain_interface::MinImageCountStrategy::MINIMUM_PLUS_ONE,
                 .chooseSurfaceFormat = {.format = VK_FORMAT_B8G8R8A8_SRGB,
                                         .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                 .imageArrayLayers = 1,
                 .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                 .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                 .queueFamilyIndices = {},
                 .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                 .choosePresentMode = VK_PRESENT_MODE_MAILBOX_KHR,
                 .clipped = VK_TRUE})
            .configImageView(
                {.viewType = VK_IMAGE_VIEW_TYPE_2D,
                 .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                 .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                      .baseMipLevel = 0,
                                      .levelCount = 1,
                                      .baseArrayLayer = 0,
                                      .layerCount = 1}})
            .create();

    // NOTE: pipeline_layout 和 graphics_pipeline 可以是多对多的关系
    const auto LAYOUT_ID = ctx.addPipelineLayout(
        create_pipeline_layout{ctx.logicalDevice()}
            .setLayouts({})
            .pushConstantRanges(                             // diff: 配置管线声明推送常量
                {{{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // 只在顶点着色器中使用
                   .offset = 0,
                   .size = sizeof(PushConstants)}}})
            .create());

    using config_shader = create_graphics_pipeline::config_shader_stage;
    const auto GRAPHICS_ID = ctx.addGraphicsPipeline(
        create_graphics_pipeline{ctx.logicalDevice()} //
            .configShaderStage(
                {config_shader{
                     .stage = VK_SHADER_STAGE_VERTEX_BIT,
                     .pName = "main",
                     .shader_info = {.filePath =
                                         VERT_SHADER_PATH}}, // diff: 带推送常量的着色器
                 config_shader{.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pName = "main",
                               .shader_info = {.filePath = FRAG_SHADER_PATH}}})
            .configVertexInputState({})
            .configAssemblyState({.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                  .primitiveRestartEnable = VK_FALSE})
            // NOTE: 使用动态渲染初始值随意
            .configViewportState({.viewports = {VkViewport{}}, .scissors = {VkRect2D{}}})
            .configRasterizationState({.depthClampEnable = VK_FALSE,
                                       .rasterizerDiscardEnable = VK_FALSE,
                                       .polygonMode = VK_POLYGON_MODE_FILL,
                                       .cullMode = VK_CULL_MODE_BACK_BIT,
                                       .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                       // .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                       .depthBiasEnable = VK_FALSE,
                                       .lineWidth = 1.0F})
            .configMultisampleState({
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // 没有硬件采样配置
                .sampleShadingEnable = VK_FALSE, // NOTE: 9. 这里可以改进内部颜色质量
            })
            .configDepthStencilState({})
            .configColorBlendState(
                {.logicOpEnable = VK_FALSE,
                 .logicOp = VkLogicOp::VK_LOGIC_OP_COPY,
                 .attachments = {VkPipelineColorBlendAttachmentState{
                     .blendEnable = VK_FALSE, // 关闭混合
                     .colorWriteMask =
                         VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}}})
            .configDynamicState(
                {.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}})
            .setLayout(ctx.getPipelineLayout(LAYOUT_ID))
            .create(structure_chain<VkPipelineRenderingCreateInfo>{
                {.colorAttachmentCount = 1,
                 .pColorAttachmentFormats = &swapChain.refImageFormat()}}));

    auto commandPool =
        CommandPool{ctx.logicalDevice(),
                    {.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                     .queueFamilyIndex = GRAPHICS_QUEUE_FAMILY_IDX}};

    std::vector<CommandBuffer> commandBuffers =
        commandPool.allocateCommandBuffers({.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                            .commandBufferCount = MAX_FRAMES_IN_FLIGHT});

    frame_context frameContext{MAX_FRAMES_IN_FLIGHT, ctx.logicalDevice(),
                               swapChain.swapChainImagesSize()};

    // NOLINTNEXTLINE
    const auto recordCommandBuffer = [&](const CommandBuffer &commandBuffer,
                                         uint32_t imageIndex) {
        VkImage image = swapChain.getImage(imageIndex);
        VkImageView imageView = swapChain.getImageView(imageIndex);
        auto imageExtent = swapChain.imageExtent();

        commandBuffer.begin({});
        my_render::transition_image_layout(
            commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {}, // srcAccessMask (no need to wait for previous operations)
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};

        VkRenderingAttachmentInfo colorAttachment = {
            .sType = sType<VkRenderingAttachmentInfo>(),
            .imageView = imageView,
            .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearColor};

        commandBuffer.beginRendering(
            {.sType = sType<VkRenderingInfo>(),
             .renderArea = {.offset = {.x = 0, .y = 0}, .extent = imageExtent},
             .layerCount = 1,
             .colorAttachmentCount = 1,
             .pColorAttachments = &colorAttachment,
             .pDepthAttachment = nullptr});
        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   ctx.getGraphicsPipeline(GRAPHICS_ID));
        commandBuffer.setViewport(0, {{.x = 0.0F,
                                       .y = 0.0F,
                                       .width = static_cast<float>(imageExtent.width),
                                       .height = static_cast<float>(imageExtent.height),
                                       .minDepth = 0.0F,
                                       .maxDepth = 1.0F}});
        commandBuffer.setScissor(0,
                                 {{.offset = {.x = 0, .y = 0}, .extent = imageExtent}});

        // diff: start: 发送顶点数据到shader
        PushConstants pushConstants{.positions = {glm::vec4(0.0f, -0.5f, 0.0f, 1.0f),
                                                  glm::vec4(0.5f, 0.5f, 0.0f, 1.0f),
                                                  glm::vec4(-0.5f, 0.5f, 0.0f, 1.0f)},
                                    .colors = {
                                        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // 白色
                                        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), // 红色
                                        // glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // 绿色
                                        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) // 蓝色
                                    }};

        ::vkCmdPushConstants(*commandBuffer, ctx.getPipelineLayout(LAYOUT_ID),
                             VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants),
                             &pushConstants);
        // diff: end

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();

        my_render::transition_image_layout(
            commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        commandBuffer.end();
    };
    // NOLINTNEXTLINE
    const auto recreateSwapChain = [&]() constexpr {
        swapChain.waitGoodFramebufferSize();
        ctx.logicalDevice().waitIdle();

        swapChain.clear();
        swapChain.recreate();
    };
    // NOLINTNEXTLINE
    const auto drawFrame = [&]() constexpr {
        auto &inFlightFences = frameContext.inFlightFences;
        auto &currentFrame = frameContext.currentFrame;
        auto &presentCompleteSemaphore = frameContext.presentCompleteSemaphore;
        auto &semaphoreIndex = frameContext.semaphoreIndex;
        auto &renderFinishedSemaphore = frameContext.renderFinishedSemaphore;

        const LogicalDevice *logicalDevice = frameContext.device_;

        while (logicalDevice->waitForFences(1, inFlightFences[currentFrame], VK_TRUE,
                                            UINT64_MAX) == VK_TIMEOUT)
            ;

        auto [result, imageIndex] = swapChain.acquireNextImage(
            UINT64_MAX, presentCompleteSemaphore[semaphoreIndex], nullptr);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("failed to acquire swap chain image!");

        logicalDevice->resetFences(1, inFlightFences[currentFrame]);

        const auto &commandBuffer = commandBuffers[currentFrame];
        commandBuffer.reset({});
        recordCommandBuffer(commandBuffer, imageIndex);

        // NOLINTNEXTLINE
        VkPipelineStageFlags waitDestinationStageMask[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        graphicsAndPresentQueue.submit(
            1,
            {.sType = sType<VkSubmitInfo>(),
             .waitSemaphoreCount = 1,
             .pWaitSemaphores = &presentCompleteSemaphore[semaphoreIndex],
             .pWaitDstStageMask = waitDestinationStageMask,
             .commandBufferCount = 1,
             .pCommandBuffers = &*commandBuffer,
             .signalSemaphoreCount = 1,
             .pSignalSemaphores = &renderFinishedSemaphore[imageIndex]},
            inFlightFences[currentFrame]);

        result = graphicsAndPresentQueue.presentKHR(
            {.sType = sType<VkPresentInfoKHR>(),
             .waitSemaphoreCount = 1,
             .pWaitSemaphores = &renderFinishedSemaphore[imageIndex],
             .swapchainCount = 1,
             .pSwapchains = &(*swapChain),
             .pImageIndices = &imageIndex});
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

    while (window.shouldClose() == 0)
    {
        surface::pollEvents();
        drawFrame();
    }
    // clear handle
    ctx.logicalDevice().waitIdle();

    commandBuffers.clear();
    commandPool.clear();
    swapChain.clear();

    std::cout << "main done\n";
    return 0;
}
catch (std::exception &e)
{
    std::println("exception: ", e.what());
}