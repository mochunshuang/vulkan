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
#include "backend/get_index_type.hpp"
#include "backend/surface_impl.hpp"
#include "backend/swap_chain_interface.hpp"
#include "backend/tools/create_buffer.hpp"
#include "backend/tools/simple_copy_buffer.hpp"
#include "backend/tools/staging_buffer.hpp"
#include "backend/wsi/glfw.hpp"
#include "backend/pick_physical_device.hpp"
#include "backend/create_logical_device.hpp"
#include "backend/sType.hpp"
#include "backend/select_queue_family_index.hpp"
#include "backend/structure_chain.hpp"
#include "backend/CommandPool.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

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

using buffer_base = mcs::vulkan::core::buffer_base;

using mcs::vulkan::tools::simple_copy_buffer;
using mcs::vulkan::tools::staging_buffer;
using mcs::vulkan::tools::create_buffer;
using mcs::vulkan::core::get_index_type;

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
// NOTE: 绘制多个网格对象
constexpr auto VERT_SHADER_PATH = "shaders/test_backend_objects_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_triangle_frag.spv";
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "test_my_triangle";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct vertex
{
    glm::vec2 pos;
    glm::vec3 color;
};
struct push_constants
{
    VkDeviceAddress vertex_buffer_address;
};

struct mesh_base
{
    using index_type = uint32_t;
    struct mesh_buffers
    {
        buffer_base vertex_buffer;
        buffer_base index_buffer;
        VkDeviceAddress vertex_buffer_address{0};
        uint32_t index_size{};
    };
    struct mesh_data
    {
        std::vector<vertex> vertices;
        std::vector<index_type> indices;
    };

    static consteval auto indexType()
    {
        return get_index_type<index_type>();
    }

    mesh_base(const CommandPool &pool, const Queue &queue, mesh_data init_data)
        : commandpool_{&pool}, queue_{&queue}, queueData_{std::move(init_data)}
    {
        for (auto &fb : frameBuffers_)
        {
            createVertexBufferForFrame(fb);
            createIndexBufferForFrame(fb);
            getBufferDeviceAddresses(fb);
        }
    }

    [[nodiscard]] VkDeviceAddress getVertexBufferAddress(
        uint32_t currentFrame) const noexcept
    {
        return frameBuffers_[currentFrame].vertex_buffer_address; // NOLINT
    }

    [[nodiscard]] VkBuffer getIndexBuffer(uint32_t currentFrame) const noexcept
    {
        return frameBuffers_[currentFrame].index_buffer.buffer(); // NOLINT
    }
    [[nodiscard]] uint32_t indexSize(uint32_t currentFrame) const noexcept
    {
        return frameBuffers_[currentFrame].index_size; // NOLINT
    }

    void queueUpdate(std::vector<vertex> vertices, std::vector<index_type> indices)
    {
        queueData_.vertices = std::move(vertices);
        queueData_.indices = std::move(indices);
        count_ = 2;
    }
    void applyQueuedUpdate(uint32_t currentFrame)
    {
        if (count_ == 0) // NOTE: main 线程安全的
        {
            queueData_.vertices.clear();
            return;
        }
        --count_;

        // 更新GPU缓冲区（这个帧当前没有被GPU使用）
        auto &fb = frameBuffers_[currentFrame]; // NOLINT
        createVertexBufferForFrame(fb);
        createIndexBufferForFrame(fb);
        getBufferDeviceAddresses(fb);
    }
    constexpr void clear() noexcept
    {
        frameBuffers_ = {};
    }

  private:
    const CommandPool *commandpool_{};
    const Queue *queue_{};
    std::array<mesh_buffers, MAX_FRAMES_IN_FLIGHT> frameBuffers_;
    mesh_data queueData_;
    uint32_t count_ = 0;

    static constexpr auto REQUIRE_BUFFER_USAGE =
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    static constexpr auto REQUIRE_MEMORY_FLAG = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    constexpr auto createBuffer(const LogicalDevice *device, VkBufferUsageFlags usage,
                                buffer_base &destBuffer, void *src,
                                VkDeviceSize buffer_size)
    {
        destBuffer = create_buffer(
            *device,
            structure_chain<VkBufferCreateInfo>{
                {.size = buffer_size,
                 .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage | REQUIRE_BUFFER_USAGE,
                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE}},
            structure_chain<VkMemoryAllocateInfo, VkMemoryAllocateFlagsInfo>{
                {}, {.flags = REQUIRE_MEMORY_FLAG}},
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        const auto STAGING_BUFFER = staging_buffer(*device, buffer_size);
        STAGING_BUFFER.copyDataToBuffer(src, buffer_size);
        simple_copy_buffer(*commandpool_, *queue_, STAGING_BUFFER.buffer(),
                           destBuffer.buffer(), {VkBufferCopy{.size = buffer_size}});
    }

    constexpr void createVertexBufferForFrame(mesh_buffers &fb)
    {
        auto &data = queueData_.vertices;
        const VkDeviceSize BUFFER_SIZE = sizeof(data[0]) * data.size();
        auto &destBuffer = fb.vertex_buffer;
        constexpr auto USAGE = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        createBuffer(commandpool_->logicalDevice(), USAGE, destBuffer, data.data(),
                     BUFFER_SIZE);
    }
    void createIndexBufferForFrame(mesh_buffers &fb)
    {
        auto &data = queueData_.indices;
        const VkDeviceSize BUFFER_SIZE = sizeof(data[0]) * data.size();
        auto &destBuffer = fb.index_buffer;
        constexpr auto USAGE = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        fb.index_size = data.size();
        createBuffer(commandpool_->logicalDevice(), USAGE, destBuffer, data.data(),
                     BUFFER_SIZE);
    }

    void getBufferDeviceAddresses(mesh_buffers &fb) const noexcept
    {
        const auto *device = commandpool_->logicalDevice();
        if (fb.vertex_buffer.buffer() != VK_NULL_HANDLE)
        {
            fb.vertex_buffer_address = device->getBufferDeviceAddress(
                {.sType = sType<VkBufferDeviceAddressInfo>(),
                 .buffer = fb.vertex_buffer.buffer()});
        }
    }
};
// diff: end

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

    // diff: start 添加扩展 和 特性
    requiredDeviceExtension.emplace_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                    VkPhysicalDeviceVulkan12Features, // diff: 添加Vulkan 1.2特性
                    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
        enablefeatureChain = {
            {.features = {.shaderInt64 = VK_TRUE}}, // diff: shader 扩展这里也要打开
            {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
            {
                .scalarBlockLayout = VK_TRUE, // diff: [new] 启用标量块布局
                .bufferDeviceAddress =
                    VK_TRUE, // diff: [new] 在Vulkan 1.3中也启用bufferDeviceAddress
            },
            {.extendedDynamicState = VK_TRUE}};
    // diff: end

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
                .requiredFeatures([](const PhysicalDevice
                                         &physicalDevice) constexpr noexcept -> bool {
                    auto query =
                        structure_chain<VkPhysicalDeviceFeatures2,
                                        VkPhysicalDeviceVulkan13Features,
                                        VkPhysicalDeviceVulkan12Features,
                                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{
                            {}, {}, {}, {}};
                    physicalDevice.getFeatures2(&query.head());
                    auto &features2 = query.template get<VkPhysicalDeviceFeatures2>();
                    auto &query_vulkan13_features =
                        query.template get<VkPhysicalDeviceVulkan13Features>();
                    auto &query_vulkan12_features = // diff: [new] 检查Vulkan 1.2特性
                        query.template get<VkPhysicalDeviceVulkan12Features>();
                    auto &query_extended_dynamic_state_features = query.template get<
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                    return features2.features.shaderInt64 && // diff: shader 需要uint64
                           query_vulkan13_features.dynamicRendering &&
                           query_vulkan13_features.synchronization2 &&
                           query_vulkan12_features
                               .bufferDeviceAddress && // diff: bufferDeviceAddress
                           query_vulkan12_features
                               .scalarBlockLayout && // diff: shader打开了scalar扩展
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
            .setLayouts({}) // diff: 管道布局声明推算常量范围
            .pushConstantRanges({{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                  .offset = 0,
                                  .size = sizeof(push_constants)}})
            .create());

    using config_shader = create_graphics_pipeline::config_shader_stage;
    const auto GRAPHICS_ID = ctx.addGraphicsPipeline(
        create_graphics_pipeline{ctx.logicalDevice()} //
            .configShaderStage(
                {config_shader{.stage = VK_SHADER_STAGE_VERTEX_BIT,
                               .pName = "main",
                               .shader_info = {.filePath = VERT_SHADER_PATH}},
                 config_shader{.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                               .pName = "main",
                               .shader_info = {.filePath = FRAG_SHADER_PATH}}})
            .configVertexInputState({})
            .configAssemblyState({.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                  .primitiveRestartEnable = VK_FALSE})
            // NOTE: 使用动态渲染初始值随意
            .configViewportState({.viewports = {VkViewport{}}, .scissors = {VkRect2D{}}})
            .configRasterizationState(
                {.depthClampEnable = VK_FALSE,
                 .rasterizerDiscardEnable = VK_FALSE,
                 .polygonMode = VK_POLYGON_MODE_FILL,
                 .cullMode = VK_CULL_MODE_NONE, // diff: 测试的时最好还是不剔除
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

    // diff: 准备好顶点和顶点索引--------------- // NOLINTBEGIN
    // 顶点数据（四边形由两个三角形组成）
    const std::vector<vertex> vertices = {
        {{-0.2f, -0.2f}, {1.0f, 0.0f, 0.0f}}, // 左下
        {{0.2, -0.2}, {0.0f, 1.0f, 0.0f}},    // 右下
        {{0.2, 0.2}, {0.0f, 0.0f, 1.0f}},     // 右上
        {{-0.2, 0.2}, {1.0f, 0.0f, 0.0f}}     // 左上
    };

    // 索引数据
    const std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    mesh_base input_mesh{commandPool, graphicsAndPresentQueue, {vertices, indices}};

    static const std::vector<vertex> VERTICES = {
        {{0.3f, -0.3f}, {1.0f, 0.0f, 0.0f}}, // 右下 - 红色
        {{0.7f, -0.3f}, {0.0f, 1.0f, 0.0f}}, // 左下 - 绿色
        {{0.5f, 0.3f}, {0.0f, 0.0f, 1.0f}}   // 上中 - 蓝色
    };
    static const std::vector<uint32_t> INDICES = {0, 1, 2};
    mesh_base input_mesh2{commandPool, graphicsAndPresentQueue, {VERTICES, INDICES}};

    // diff: end // NOLINTEND

    // NOLINTNEXTLINE  // diff: 增加 currentFrame 参数
    const auto recordCommandBuffer = [&](const CommandBuffer &commandBuffer,
                                         uint32_t currentFrame, uint32_t imageIndex) {
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
        // diff: begin 即使使用设备地址，Vulkan仍需要绑定索引缓冲区.来遍历顶点数组
        {
            auto &render = input_mesh;
            push_constants pushConstants = {
                .vertex_buffer_address = render.getVertexBufferAddress(currentFrame)};

            // NOTE: 当前仅仅推送顶点数据
            commandBuffer.pushConstants(ctx.getPipelineLayout(0),
                                        VK_SHADER_STAGE_VERTEX_BIT, 0,
                                        sizeof(push_constants), &pushConstants);
            commandBuffer.bindIndexBuffer(render.getIndexBuffer(currentFrame), 0,
                                          mesh_base::indexType());

            commandBuffer.drawIndexed(render.indexSize(currentFrame), 1, 0, 0, 0);
        }
        {
            auto &render = input_mesh2;
            push_constants pushConstants = {
                .vertex_buffer_address = render.getVertexBufferAddress(currentFrame)};

            // NOTE: 当前仅仅推送顶点数据
            commandBuffer.pushConstants(ctx.getPipelineLayout(0),
                                        VK_SHADER_STAGE_VERTEX_BIT, 0,
                                        sizeof(push_constants), &pushConstants);

            commandBuffer.bindIndexBuffer(render.getIndexBuffer(currentFrame), 0,
                                          mesh_base::indexType());

            commandBuffer.drawIndexed(render.indexSize(currentFrame), 1, 0, 0, 0);
        }
        // diff: end

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

        // diff: vkResetFences 之后更新顶点. 仅仅更新飞行的帧
        input_mesh.applyQueuedUpdate(currentFrame);
        recordCommandBuffer(commandBuffer, currentFrame, imageIndex);
        // diff: end

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