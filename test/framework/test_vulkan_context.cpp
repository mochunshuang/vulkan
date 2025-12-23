#include "./head.hpp"

#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <print>
#include <utility>
#include <chrono>

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

using swap_chain = mcs::vulkan::swap_chain;
using color_image = mcs::vulkan::color_image;
using deep_image = mcs::vulkan::deep_image;

using mcs::vulkan::structure_chain;
using mcs::vulkan::sType;

using mcs::vulkan::context_wsi;

using mcs::vulkan::make_color_image;
using mcs::vulkan::make_deep_image;
using mcs::vulkan::make_swap_chain;
using mcs::vulkan::make_descriptor_resource;
using mcs::vulkan::make_texture_image;

template <typename ContextWsi>
struct my_swap_chain
{
    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return swapChain_.valid() && colorImage_.valid() && deepImage_.valid();
    }

    constexpr my_swap_chain(make_swap_chain<ContextWsi> swapChainBuild,
                            make_color_image<ContextWsi> colorImageBuild,
                            make_deep_image<ContextWsi> deepImageBuild)
        : swapChainBuild_{swapChainBuild}, colorImageBuild_{colorImageBuild},
          deepImageBuild_{deepImageBuild}, swapChain_{swapChainBuild_.build()},
          colorImage_{colorImageBuild.build()}, deepImage_{deepImageBuild_.build()}
    {
        mcs::MCS_ASSERT(valid());
    }

    constexpr auto &ref_swapChain() const noexcept // NOLINT
    {
        return swapChain_;
    }

    constexpr auto &ref_colorImage() const noexcept // NOLINT
    {
        return colorImage_;
    }

    constexpr auto &ref_deepImage() const noexcept // NOLINT
    {
        return deepImage_;
    }

    constexpr void recreateSwapChain() &
    {
        swapChainBuild_.context()->surfaceImpl()->waitGoodFramebufferSize();
        swapChainBuild_.context()->ref_logical_device().waitIdle();

        clear();

        // 打印当前帧缓冲区大小
        auto cur_chain_extent =
            swapChainBuild_.context()->surfaceImpl()->getFramebufferSize();
        std::cout << "Recreating swap chain with size: " << cur_chain_extent.width << "x"
                  << cur_chain_extent.height << std::endl;

        swapChain_ = swapChainBuild_.setSurfaceExtent(cur_chain_extent).build();
        colorImage_ = colorImageBuild_.setSurfaceExtent(cur_chain_extent).build();
        deepImage_ = deepImageBuild_.setSurfaceExtent(cur_chain_extent).build();
    }

    [[nodiscard]] const VkExtent2D &ref_surfaceExtent() const noexcept // NOLINT
    {
        return swapChainBuild_.ref_surfaceExtent();
    }

    [[nodiscard]] auto *context() const noexcept
    {
        return swapChainBuild_.context();
    }

  private:
    make_swap_chain<ContextWsi> swapChainBuild_;
    make_color_image<ContextWsi> colorImageBuild_;
    make_deep_image<ContextWsi> deepImageBuild_;
    swap_chain swapChain_;
    color_image colorImage_;
    deep_image deepImage_;

    constexpr void clear() noexcept
    {
        deepImage_ = {};
        colorImage_ = {};
        swapChain_ = {};
    }
};

//-------------pipeline-------------------------
struct vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord; // NOTE: 5. 增加顶点的 纹理坐标，“uv坐标”。// NOLINT

    static_assert(sizeof(float) == 4);

    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(vertex),
                .inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return {
            VkVertexInputAttributeDescription{.location = 0,
                                              .binding = 0,
                                              .format =
                                                  VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
                                              .offset = offsetof(vertex, pos)},
            VkVertexInputAttributeDescription{.location = 1,
                                              .binding = 0,
                                              .format =
                                                  VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
                                              .offset = offsetof(vertex, color)},
            VkVertexInputAttributeDescription{.location = 2,
                                              .binding = 0,
                                              .format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
                                              .offset = offsetof(vertex, texCoord)}};
    }

    bool operator==(const vertex &other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};
template <>
struct std::hash<vertex>
{
    size_t operator()(vertex const &vertex) const noexcept
    {
        return ((hash<glm::vec3>()(vertex.pos) ^
                 (hash<glm::vec3>()(vertex.color) << 1)) >>
                1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};

using mcs::vulkan::descriptor_resource;
using mcs::vulkan::context_base;
using mcs::vulkan::shader_module;

struct my_graphics_pipeline
{

    [[nodiscard]] bool valid() const noexcept
    {
        return descriptorResource_.valid() && pipelineLayout_ != nullptr &&
               graphicsPipeline_ != nullptr;
    }

    constexpr my_graphics_pipeline(const logical_device &device,
                                   descriptor_resource &&descriptor_resource,
                                   VkPipelineLayout pipelineLayout,
                                   VkPipeline graphicsPipeline) noexcept
        : device_{&device}, descriptorResource_{std::move(descriptor_resource)},
          pipelineLayout_{pipelineLayout}, graphicsPipeline_{graphicsPipeline}
    {
    }

    [[nodiscard]] auto *device() const noexcept
    {
        return device_;
    }
    [[nodiscard]] auto &ref_descriptorResource() const noexcept // NOLINT
    {
        return descriptorResource_;
    }
    [[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept
    {
        return pipelineLayout_;
    }
    [[nodiscard]] VkPipeline graphicsPipeline() const noexcept
    {
        return graphicsPipeline_;
    }

    constexpr ~my_graphics_pipeline() noexcept
    {
        destroy();
    }
    constexpr my_graphics_pipeline(my_graphics_pipeline &&o) noexcept
        : device_{std::exchange(o.device_, {})},
          descriptorResource_{std::move(o.descriptorResource_)},
          pipelineLayout_{std::exchange(o.pipelineLayout_, {})},
          graphicsPipeline_{std::exchange(o.graphicsPipeline_, {})}
    {
    }
    constexpr my_graphics_pipeline &operator=(my_graphics_pipeline &&o) noexcept
    {
        if (&o != this)
        {
            this->destroy();
            device_ = std::exchange(o.device_, {});
            descriptorResource_ = std::move(o.descriptorResource_);
            pipelineLayout_ = std::exchange(o.pipelineLayout_, {});
            graphicsPipeline_ = std::exchange(o.graphicsPipeline_, {});
        }
        return *this;
    }

    my_graphics_pipeline(const my_graphics_pipeline &) = delete;
    my_graphics_pipeline &operator=(const my_graphics_pipeline &) = delete;

  private:
    const logical_device *device_{};
    descriptor_resource descriptorResource_;
    VkPipelineLayout pipelineLayout_ = nullptr;
    VkPipeline graphicsPipeline_ = nullptr;

    constexpr void destroy() noexcept
    {
        if (device_ != nullptr)
        {
            if (graphicsPipeline_ != nullptr)
            {
                device_->destroyPipeline(graphicsPipeline_, nullptr);
                graphicsPipeline_ = nullptr;
            }
            if (pipelineLayout_ != nullptr)
            {
                device_->destroyPipelineLayout(pipelineLayout_, nullptr);
                pipelineLayout_ = nullptr;
            }
            descriptorResource_ = {};
            device_ = {};
        }
    }
};

static constexpr my_graphics_pipeline createGraphicsPipeline(
    context_base &ctx, descriptor_resource &descriptor_resource, VkFormat colorFormat,
    VkFormat depthFormat, VkSampleCountFlagBits msaaSamples)
{
    const auto &device = ctx.ref_logical_device();
    auto *descriptorSetLayout = descriptor_resource.descriptorSetLayout();

    shader_module shaderModule = shader_module(device, "shaders/deep.spv");
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = sType<VkPipelineShaderStageCreateInfo>(),
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = shaderModule.raw_data(),
        .pName = "vertMain"};
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = sType<VkPipelineShaderStageCreateInfo>(),
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shaderModule.raw_data(),
        .pName = "fragMain"};

    // NOLINTNEXTLINE
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};
    auto bindingDescription = vertex::getBindingDescription();
    auto attributeDescriptions = vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription, // NOTE: 绑定
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = sType<VkPipelineInputAssemblyStateCreateInfo>(),
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE};

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = sType<VkPipelineViewportStateCreateInfo>(),
        .viewportCount = 1,
        .scissorCount = 1};

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        // .cullMode = VK_CULL_MODE_BACK_BIT,
        // .frontFace = VK_FRONT_FACE_CLOCKWISE,
        // .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0F};

    // c15: 渲染管道也要传入采样
    // MSAA仅平滑几何边缘，但没有平滑内部填充。
    // 启用样本着色，这将进一步改善画质，尽管会增加性能成本
    // 在某些情况下，质量改进可能会很明显
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
        .rasterizationSamples = msaaSamples,
        // NOTE: 9. 这里可以改进内部颜色质量
        .sampleShadingEnable = VK_FALSE,
    };
    // diff:--------------
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_TRUE, // 启用混合
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = sType<VkPipelineColorBlendStateCreateInfo>(),
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    // c11: 深度附件现在可以使用了，但是深度测试仍然需要在图形管道中启用。
    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE};

    // NOTE: 动态的意思是 cmd 的时候需要指定，确定动态类型
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE, VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = sType<VkPipelineDynamicStateCreateInfo>(),
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = sType<VkPipelineLayoutCreateInfo>(),
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout}; // c8: 添加描述符集

    VkPipelineLayout pipelineLayout = nullptr; // NOLINT
    VkPipeline graphicsPipeline = nullptr;     // NOLINT
    try
    {
        pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo, nullptr);

        // Use dynamic rendering
        mcs::vulkan::structure_chain<VkGraphicsPipelineCreateInfo,
                                     VkPipelineRenderingCreateInfo>
            pipelineCreateInfoChain = {
                {.stageCount = 2,
                 .pStages = shaderStages,
                 .pVertexInputState = &vertexInputInfo,
                 .pInputAssemblyState = &inputAssembly,
                 .pViewportState = &viewportState,
                 .pRasterizationState = &rasterizer,
                 .pMultisampleState = &multisampling, // c15: MSAA
                 .pDepthStencilState = &depthStencil, // c11: 注入管道
                 .pColorBlendState = &colorBlending,
                 .pDynamicState = &dynamicState,
                 .layout = pipelineLayout,
                 .renderPass = VK_NULL_HANDLE},
                {.colorAttachmentCount = 1,
                 .pColorAttachmentFormats = &colorFormat,
                 .depthAttachmentFormat = depthFormat}}; // c11: 启用深度模板测试

        graphicsPipeline = device.createGraphicsPipelines(
            nullptr, 1, pipelineCreateInfoChain.head(), nullptr);

        return my_graphics_pipeline{device, std::move(descriptor_resource),
                                    pipelineLayout, graphicsPipeline};
    }
    catch (...)
    {
        if (graphicsPipeline != nullptr)
        {
            device.destroyPipeline(graphicsPipeline, nullptr);
            graphicsPipeline = nullptr;
        }
        if (pipelineLayout != nullptr)
        {
            device.destroyPipelineLayout(pipelineLayout, nullptr);
            pipelineLayout = nullptr;
        }
        throw;
    }
}

struct frame_context
{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2; // NOLINT

    // NOLINTBEGIN
    const logical_device *device_{};
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> presentCompleteSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::vector<VkFence> inFlightFences;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;
    // NOLINTEND

    explicit frame_context(context_base &ctx, size_t swapChainImagesSize)
        : device_{&ctx.ref_logical_device()}
    {
        createCommandBuffers(ctx);
        createSyncObjects(ctx, swapChainImagesSize);
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
    void createCommandBuffers(context_base &ctx)
    {
        auto *device = ctx.raw_logical_device();
        auto *commandPool = ctx.defalutCommandPool();

        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo = {
            .sType = sType<VkCommandBufferAllocateInfo>(),
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())};

        // TODO(mcs): 看起来要先分配内存
        if (::vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) !=
            VK_SUCCESS)
            throw std::runtime_error("failed to allocate command buffers!");
    }
    void createSyncObjects(context_base &ctx, size_t swapChainImagesSize)
    {
        auto *device = ctx.raw_logical_device();

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
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
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
        inFlightFences.clear();
    }

    constexpr void destroy() noexcept
    {
        if (device_ != nullptr)
        {
            destroySyncObject();
            // commandBuffers 不包括和奇怪。和 commandPool 绑定的
            device_ = nullptr;
        }
    }
};

struct mesh_data
{
    // NOLINTBEGIN
    const context_base *context;
    std::vector<vertex> vertices;
    std::vector<uint32_t> indices;
    mcs::vulkan::buffer_base vertexBuffer;
    mcs::vulkan::buffer_base indexBuffer;
    // NOLINTEND

    explicit mesh_data(context_base &context, std::vector<vertex> vertices,
                       std::vector<uint32_t> indices)
        : context{&context}, vertices{std::move(vertices)}, indices{std::move(indices)}
    {
        setup();

        // NOTE: 索引缓冲区依赖。也不能迁移到外面，无法分离
        //  第二次同步崩溃。 无法更新。 只能整个重建
        //  auto a = sizeof(vertices[0]) * vertices.size();
        //  auto b = sizeof(indices[0]) * indices.size();
        //  syncStatus(vertexBuffer, vertices.data(), a);
        //  syncStatus(indexBuffer, indices.data(), b);

        // NOTE: 两次setup 不崩溃
        setup();
    }

    void update()
    {
        setup();
    }

  private:
    void setup()
    {
        createVertexBuffer();
        createIndexBuffer();
    }

    void syncStatus(const mcs::vulkan::buffer_base &buffer, void *data,
                    const VkDeviceSize &BUFFER_SIZE) const
    {
        const auto &device = context->ref_logical_device();
        const auto &physicalDevice = context->ref_physical_device();
        auto *queue = context->defaultQueue();
        auto *commandPool = context->defalutCommandPool();

        mcs::vulkan::staging_buffer stagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {
                                           .sType = sType<VkBufferCreateInfo>(),
                                           .size = BUFFER_SIZE,
                                           .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                       },
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        stagingBuffer.mapAndUnmapMempry(data, static_cast<size_t>(BUFFER_SIZE));

        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 buffer.buffer(), BUFFER_SIZE);
    }

    void createVertexBuffer()
    {
        const auto &device = context->ref_logical_device();
        const auto &physicalDevice = context->ref_physical_device();

        const VkDeviceSize BUFFER_SIZE = sizeof(vertices[0]) * vertices.size();

        vertexBuffer =
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {
                                           .sType = sType<VkBufferCreateInfo>(),
                                           .size = BUFFER_SIZE,
                                           .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                       },
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // NOTE: 无法搬迁到外面很离谱。耦合性非常高可能同一时间只能创建并使用一个buffer?
        syncStatus(vertexBuffer, vertices.data(), BUFFER_SIZE);
    }

    void createIndexBuffer()
    {
        const auto &device = context->ref_logical_device();
        const auto &physicalDevice = context->ref_physical_device();

        const VkDeviceSize BUFFER_SIZE = sizeof(indices[0]) * indices.size();
        indexBuffer =
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {
                                           .sType = sType<VkBufferCreateInfo>(),
                                           .size = BUFFER_SIZE,
                                           .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                       },
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        // NOTE: 无法搬迁到外面很离谱
        syncStatus(indexBuffer, indices.data(), BUFFER_SIZE);
    }

  public:
    void bind(VkCommandBuffer commandBuffer) const noexcept
    {
        auto *vertex = vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0}; // NOLINT
        ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, offsets);
        ::vkCmdBindIndexBuffer(
            commandBuffer, indexBuffer.buffer(), 0,
            VK_INDEX_TYPE_UINT32); // c12: 要匹配上：uint32_t 顶点索引类型
    }

    void draw(VkCommandBuffer commandBuffer) const noexcept
    {
        ::vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0,
                           0);
    }
};

// Uniform Buffer对象结构 // NOLINTBEGIN
struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
}; // NOLINTEND
struct uniform_buffer
{
    // NOLINTBEGIN
    std::vector<mcs::vulkan::buffer_base> buffers;
    std::vector<void *> mapped;
    // NOLINTEND

    constexpr uniform_buffer(context_base &ctx, size_t swapChainSize)
    {
        const auto &device = ctx.ref_logical_device();
        const auto &physicalDevice = ctx.ref_physical_device();

        buffers.resize(swapChainSize);
        mapped.resize(swapChainSize);

        constexpr VkDeviceSize BUFFER_SIZE = sizeof(UniformBufferObject);
        constexpr VkBufferCreateInfo CREATE_INFO = {
            .sType = sType<VkBufferCreateInfo>(),
            .size = BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        constexpr VkMemoryPropertyFlags PROPERTIES =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        for (size_t i = 0; i < swapChainSize; i++)
        {
            buffers[i] = mcs::vulkan::create_buffer(physicalDevice, device, CREATE_INFO,
                                                    PROPERTIES);
            ::vkMapMemory(device.raw_data(), buffers[i].bufferMemory(), 0, BUFFER_SIZE, 0,
                          &mapped[i]);
        }
    }
    constexpr ~uniform_buffer() noexcept
    {
        destroy();
    }

    uniform_buffer(uniform_buffer &&o) noexcept
        : buffers{std::move(o.buffers)}, mapped{std::move(o.mapped)} {

          };

    uniform_buffer &operator=(uniform_buffer &&o) noexcept
    {
        if (&o != this)
        {
            this->destroy();
            buffers = std::move(o.buffers);
            mapped = std::move(o.mapped);
        }
        return *this;
    }
    uniform_buffer(const uniform_buffer &) = delete;
    uniform_buffer &operator=(const uniform_buffer &) = delete;

    void update(uint32_t currentImage, VkExtent2D swapChainExtent) const
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();

        // NOLINTBEGIN
        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                                glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f),
                                    static_cast<float>(swapChainExtent.width) /
                                        static_cast<float>(swapChainExtent.height),
                                    0.1f, 10.0f);
        // NOLINTEND
        ubo.proj[1][1] *= -1;

        ::memcpy(mapped[currentImage], &ubo, sizeof(ubo));
    }

  private:
    constexpr void destroy() noexcept
    {
        if (auto size = buffers.size(); size > 0)
        {
            auto *device = buffers[0].device()->raw_data();
            for (size_t i = 0; i < size; i++)
            {
                if (mapped[i] != nullptr)
                    ::vkUnmapMemory(device, buffers[i].bufferMemory());
            }
        }
        buffers.clear();
        mapped.clear();
    }
};

struct render_object
{
    // 图元类型枚举
    enum class PrimitiveTopology : std::uint8_t
    {
        TRIANGLE_LIST,
        TRIANGLE_STRIP,
        TRIANGLE_FAN,
        LINE_LIST,
        LINE_STRIP,
        POINT_LIST
    };
    // 获取对应的Vulkan图元拓扑类型
    static VkPrimitiveTopology getVkPrimitiveTopology(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::TRIANGLE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TRIANGLE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::TRIANGLE_FAN:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case PrimitiveTopology::LINE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LINE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::POINT_LIST:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        default:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    // 添加图元拓扑成员
    PrimitiveTopology primitiveTopology = PrimitiveTopology::TRIANGLE_LIST; // NOLINT

    render_object(mesh_data mesh, uniform_buffer uniformBuffer,
                  mcs::vulkan::texture_image textureImage,
                  std::vector<VkDescriptorSet> descriptorSets) noexcept
        : mesh_{std::move(mesh)}, uniformBuffer_{std::move(uniformBuffer)},
          textureImage_{std::move(textureImage)},
          descriptorSets_{std::move(descriptorSets)}
    {
        updateDescriptorSets(textureImage_.device());
    }

    // c19: 在draw函数中使用
    void draw(VkCommandBuffer cmd, uint32_t currentImage, VkPipelineLayout pipelineLayout)
    {
        ::vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                                  1, &descriptorSets_[currentImage], 0, nullptr);

        // 设置图元拓扑
        ::vkCmdSetPrimitiveTopology(cmd, getVkPrimitiveTopology(primitiveTopology));

        mesh_.bind(cmd);
        mesh_.draw(cmd);
    }
    void update(uint32_t currentImage, VkExtent2D swapChainExtent) const
    {
        uniformBuffer_.update(currentImage, swapChainExtent);
    }

    auto &ref_mesh() noexcept // NOLINT
    {
        return mesh_;
    }

  private:
    mesh_data mesh_;
    uniform_buffer uniformBuffer_;
    mcs::vulkan::texture_image textureImage_;
    std::vector<VkDescriptorSet> descriptorSets_;

    void updateDescriptorSets(const logical_device *device) noexcept
    {
        for (size_t i = 0; i < descriptorSets_.size(); i++)
        {
            VkDescriptorBufferInfo bufferInfo = {.buffer =
                                                     uniformBuffer_.buffers[i].buffer(),
                                                 .offset = 0,
                                                 .range = sizeof(UniformBufferObject)};

            // C9: 更新到描述及。 就是通过 imageView 和 sampler 传给sharder的
            VkDescriptorImageInfo imageInfo{
                .sampler = textureImage_.sampler(),
                .imageView = textureImage_.imageView(),
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            std::array descriptorWrites = {
                VkWriteDescriptorSet{.sType = sType<VkWriteDescriptorSet>(),
                                     .dstSet = descriptorSets_[i],
                                     .dstBinding = 0,
                                     .dstArrayElement = 0,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     .pBufferInfo = &bufferInfo},
                VkWriteDescriptorSet{.sType = sType<VkWriteDescriptorSet>(),
                                     .dstSet = descriptorSets_[i],
                                     .dstBinding = 1, // index + 1
                                     .dstArrayElement = 0,
                                     .descriptorCount = 1,
                                     .descriptorType =
                                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     .pImageInfo = &imageInfo}};
            device->updateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(),
                                         0, nullptr);
        }
    }
};

struct my_render
{
    static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                      VkImageLayout oldLayout, VkImageLayout newLayout,
                                      VkAccessFlags srcAccessMask,
                                      VkAccessFlags dstAccessMask, // NOLINT
                                      VkPipelineStageFlags srcStageMask,
                                      VkPipelineStageFlags dstStageMask, // NOLINT
                                      VkImageAspectFlags aspectMask)
    {
        VkImageMemoryBarrier barrier = {
            .sType = sType<VkImageMemoryBarrier>(),
            // Specify the pipeline stages and access masks for the barrier
            .srcAccessMask = srcAccessMask,
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
        ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr,
                               0, nullptr, 1, &barrier);
    }

    static void recordCommandBuffer(my_graphics_pipeline &graphics_pipeline,
                                    my_swap_chain<context_wsi<surface>> &my_swapchain,
                                    std::vector<render_object> &objects,
                                    VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                    const VkExtent2D &swapChainExtent)
    {

        const auto &swapChainImages = my_swapchain.ref_swapChain().ref_swapChainImages();
        const auto &swapChainImageViews =
            my_swapchain.ref_swapChain().ref_swapChainImageViews();

        auto *graphicsPipeline = graphics_pipeline.graphicsPipeline();

        auto *depthImage = my_swapchain.ref_deepImage().image();
        auto *depthImageView = my_swapchain.ref_deepImage().imageView();

        auto *colorImage = my_swapchain.ref_colorImage().image();
        auto *colorImageView = my_swapchain.ref_colorImage().imageView();

        VkCommandBufferBeginInfo beginInfo = {.sType = sType<VkCommandBufferBeginInfo>()};

        if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // Before starting rendering, transition the swapchain image to
        // COLOR_ATTACHMENT_OPTIMAL
        transitionImageLayout(
            commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {}, // srcAccessMask (no need to wait for previous operations)
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        // c15: Transition the multisampled color image to COLOR_ATTACHMENT_OPTIMAL
        transitionImageLayout(
            commandBuffer, colorImage, VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
            VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);

        // c11: 将深度图像附加添加到布局中
        transitionImageLayout(
            commandBuffer, depthImage, VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
            VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};
        // c15: 修改颜色参数，添加颜色采样。这就是添加颜色附加了
        VkRenderingAttachmentInfo colorAttachment = {
            .sType = sType<VkRenderingAttachmentInfo>(),
            .imageView = colorImageView,
            .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VkResolveModeFlagBits::VK_RESOLVE_MODE_AVERAGE_BIT,
            .resolveImageView = swapChainImageViews[imageIndex],
            .resolveImageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearColor};

        // C11: 深度清除值值
        VkClearValue clearDepth = {.depthStencil = {.depth = 1.0F, .stencil = 0}};
        VkRenderingAttachmentInfo depthAttachmentInfo = {
            .sType = sType<VkRenderingAttachmentInfo>(),
            .imageView = depthImageView,
            .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = clearDepth};

        VkRenderingInfo renderingInfo = {
            .sType = sType<VkRenderingInfo>(),
            .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
            .pDepthAttachment = &depthAttachmentInfo}; // C11: 渲染包含包含深度附件

        ::vkCmdBeginRendering(commandBuffer, &renderingInfo);

        ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            graphicsPipeline);

        // c12: 动态状态设置CullMode、FrontFace 是动态属性 VK_CULL_MODE_BACK_BIT
        vkCmdSetCullMode(commandBuffer, VK_CULL_MODE_NONE); // c19: 立方体
        vkCmdSetFrontFace(commandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        VkViewport viewport = {.x = 0.0F,
                               .y = 0.0F,
                               .width = static_cast<float>(swapChainExtent.width),
                               .height = static_cast<float>(swapChainExtent.height),
                               .minDepth = 0.0F,
                               .maxDepth = 1.0F};
        ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
        ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        for (auto &obj : objects)
        {
            obj.update(imageIndex, swapChainExtent);
            obj.draw(commandBuffer, imageIndex, graphics_pipeline.pipelineLayout());
        }

        ::vkCmdEndRendering(commandBuffer);

        // Transition image layout for presentation
        transitionImageLayout(
            commandBuffer, swapChainImages[imageIndex],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        if (::vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
    static void drawFrame(frame_context &frame_context,
                          my_graphics_pipeline &graphics_pipeline,
                          my_swap_chain<context_wsi<surface>> &my_swapchain,
                          std::vector<render_object> &objects)
    {

        auto *swapChain = my_swapchain.ref_swapChain().swapChainKHR();
        auto *context = my_swapchain.context();
        auto *device = context->raw_logical_device();
        auto *queue = context->defaultQueue();

        auto *window = context->surfaceImpl();

        auto &inFlightFences = frame_context.inFlightFences;
        auto &currentFrame = frame_context.currentFrame;
        auto &presentCompleteSemaphore = frame_context.presentCompleteSemaphore;
        auto &semaphoreIndex = frame_context.semaphoreIndex;
        auto &commandBuffers = frame_context.commandBuffers;
        auto &renderFinishedSemaphore = frame_context.renderFinishedSemaphore;

        ::vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex; // NOLINT
        VkResult result = ::vkAcquireNextImageKHR(
            device, swapChain, UINT64_MAX, presentCompleteSemaphore[semaphoreIndex],
            VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            my_swapchain.recreateSwapChain();
            return;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        ::vkResetFences(device, 1, &inFlightFences[currentFrame]);
        ::vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        recordCommandBuffer(graphics_pipeline, my_swapchain, objects,
                            commandBuffers[currentFrame], imageIndex,
                            my_swapchain.ref_surfaceExtent()); // c30: 全局

        // NOLINTNEXTLINE
        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submitInfo = {
            .sType = sType<VkSubmitInfo>(),
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &presentCompleteSemaphore[semaphoreIndex],
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderFinishedSemaphore[imageIndex]};

        if (::vkQueueSubmit(queue, 1, &submitInfo, inFlightFences[currentFrame]) !=
            VK_SUCCESS)
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

        result = ::vkQueuePresentKHR(queue, &presentInfo);

        if (auto &framebufferResized = window->refFramebufferResized();
            result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
            framebufferResized)
        {
            framebufferResized = false;
            my_swapchain.recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
            throw std::runtime_error("failed to present swap chain image!");

        semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
        currentFrame = (currentFrame + 1) % frame_context::MAX_FRAMES_IN_FLIGHT;
    }
};

int main()
{
    // mcs::vulkan::wsi::glfw::Window
    try
    {

        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}; // NOLINTEND

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan11Features,
                        VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                // C9:  检查是否支持各向异性采样。纹理映射需要
                {.features = {.samplerAnisotropy = VK_TRUE}},
                {.shaderDrawParameters = VK_TRUE},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

        // NOTE: 0. surfaceimpl -----------------------------------------------
        surface window{};
        window.setup({.width = 800, .height = 600}, "test"); // NOLINT

        // NOTE: 1. context_base -----------------------------------------------
        mcs::vulkan::context_base ctx;
        ctx.createInstance(
               make_instance{}
                   .enableDebugExtension()
                   .enableSurfaceExtension<surface>()
                   .checkExtensionSupport()
                   .checkLayerSupport()
                   .build({.sType = sType<VkApplicationInfo>(),
                           .pApplicationName = "Hello Triangle",
                           .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                           .pEngineName = "No Engine",
                           .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                           // apiVersion必须是应用程序设计使用的Vulkan的最高版本
                           .apiVersion = VK_API_VERSION_1_3}))
            .createSurface(window)
            .createPhysicalDevice(
                make_physical_device{ctx.raw_instance()}
                    .requiredDeviceProperties(
                        [](const VkPhysicalDeviceProperties
                               &device_properties) constexpr noexcept {
                            return device_properties.apiVersion >= VK_API_VERSION_1_3;
                        })
                    .requiredQueueFamilyProperties(
                        [](const VkQueueFamilyProperties &qfp) constexpr noexcept {
                            return !!(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT);
                        })
                    .requiredDeviceExtensions(requiredDeviceExtension)
                    .requiredFeatures([](const physical_device
                                             &physicalDevice) constexpr noexcept -> bool {
                        auto query = structure_chain<
                            VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{{}, {}, {}};
                        physicalDevice.getFeatures2(query.head());
                        auto &query_vulkan13_features =
                            query.template get<VkPhysicalDeviceVulkan13Features>();
                        auto &query_extended_dynamic_state_features = query.template get<
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                        return query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
                               query_extended_dynamic_state_features.extendedDynamicState;
                    })
                    .pickPhysicalDevice())
            .createLogicalDevice(
                make_logical_device{}
                    .setQueuePriority(1)
                    .requiredDeviceCreateInfo(
                        [&](VkDeviceQueueCreateInfo &queueCreateInfo)
                            -> VkDeviceCreateInfo {
                            return {
                                .sType = sType<VkDeviceCreateInfo>(),
                                .pNext = &enablefeatureChain.head(),
                                .queueCreateInfoCount = 1,
                                .pQueueCreateInfos = &queueCreateInfo,
                                .enabledExtensionCount =
                                    static_cast<uint32_t>(requiredDeviceExtension.size()),
                                .ppEnabledExtensionNames = requiredDeviceExtension.data(),
                            };
                        })
                    .afterQueueCreateInfoInit([]([[maybe_unused]] VkDeviceQueueCreateInfo
                                                     &queueCreateInfo) noexcept {})
                    .afterBuildSuccess([&](const logical_device &logicalDevice,
                                           uint32_t queueFamilyIndex) {
                        // use queueFamilyIndex
                        constexpr auto QUEUE_INDEX = 0;
                        ctx.createDefaultQueue(logicalDevice.getDeviceQueue(
                                                   queueFamilyIndex, QUEUE_INDEX))
                            .createDefalutCommandPool(logicalDevice.createCommandPool(
                                {.sType = sType<VkCommandPoolCreateInfo>(),
                                 .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                 .queueFamilyIndex = queueFamilyIndex}));
                    })
                    .build(make_queue_family_index{ctx.ref_physical_device()}
                               .requiredQueueFamilyProperties(
                                   [&](const VkQueueFamilyProperties &qfp,
                                       uint32_t queueFamilyIndex,
                                       const physical_device &physicalDevice) noexcept
                                       -> bool {
                                       return (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                                              physicalDevice.getSurfaceSupportKHR(
                                                  queueFamilyIndex, ctx.surface());
                                   })));
        mcs::MCS_ASSERT(ctx.ref_logical_device().valid());
        mcs::MCS_ASSERT(ctx.defaultQueue() != nullptr);

        mcs::MCS_ASSERT(ctx.valid());

        // NOTE: 2. context_wsi -----------------------------------------------
        context_wsi<surface> context_with_surface{ctx, window};

        // NOTE: 3. my_swapchain -----------------------------------------------
        //  c30: ---- 必须是全局的，否则会出现不一致，实时采样有区别就是错误---------
        // g_swap_chain_extent = context_with_surface.surfaceExtent(); //NOTE:注入替代

        my_swap_chain<context_wsi<surface>> my_swapchain = {
            // 1. swap_chain
            make_swap_chain<context_wsi<surface>>{context_with_surface}
                .setSurfaceExtent(context_with_surface.surfaceExtent())
                .requiredSwapchainCreateInfoKHR(
                    [](make_swap_chain<context_wsi<surface>> *self)
                        -> VkSwapchainCreateInfoKHR {
                        auto *ctx = self->context();
                        // c30: swapChainExtent所有图片必须是一致的，不能各自实时避免差异
                        const VkExtent2D &swapChainExtent = self->ref_surfaceExtent();

                        auto swapChainSurfaceFormat = ctx->chooseSurfaceFormat(
                            {.format = VK_FORMAT_B8G8R8A8_SRGB,
                             .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

                        auto minImageCount = ctx->minImageCount();

                        // chooseSwapPresentMode
                        auto presentMode =
                            ctx->choosePresentMode(VK_PRESENT_MODE_MAILBOX_KHR);

                        auto surfaceCapabilities = ctx->getSurfaceCapabilitiesKHR();
                        return {.sType = sType<VkSwapchainCreateInfoKHR>(),
                                .surface = ctx->surface(),
                                .minImageCount = minImageCount,
                                .imageFormat = swapChainSurfaceFormat.format,
                                .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                                .imageExtent = swapChainExtent,
                                .imageArrayLayers = 1,
                                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                .preTransform = surfaceCapabilities.currentTransform,
                                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                .presentMode = presentMode,
                                .clipped = VK_TRUE};
                    })
                .requiredImageViewCreateInfo(
                    [](const VkImage &image,
                       const VkSwapchainCreateInfoKHR &imageCreateInfo) noexcept
                        -> VkImageViewCreateInfo {
                        return {
                            .sType = sType<VkImageViewCreateInfo>(),
                            .image = image,
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = imageCreateInfo.imageFormat,
                            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                 .baseMipLevel = 0,
                                                 .levelCount = 1,
                                                 .baseArrayLayer = 0,
                                                 .layerCount = 1}};
                    }),
            // 2. color_image
            make_color_image<context_wsi<surface>>{context_with_surface}
                .setSurfaceExtent(context_with_surface.surfaceExtent())
                .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                .requiredImageCreateInfo([](make_color_image<context_wsi<surface>> *self)
                                             -> VkImageCreateInfo {
                    auto surfaceFormats = self->context()->getSurfaceFormatsKHR();
                    auto msaaSamples = self->context()->getMaxUsableSampleCount();

                    // NOTE: 得到实时窗口大小. color_image,deep_image,swapchain需要一致
                    const VkExtent2D &swapChainExtent = self->ref_surfaceExtent();

                    auto swapChainSurfaceFormat = self->context()->chooseSurfaceFormat(
                        {.format = VK_FORMAT_B8G8R8A8_SRGB,
                         .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

                    auto colorFormat = swapChainSurfaceFormat.format;

                    return {.sType = sType<VkImageCreateInfo>(),
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = colorFormat,
                            .extent = {.width = swapChainExtent.width,
                                       .height = swapChainExtent.height,
                                       .depth = 1},
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = msaaSamples,
                            .tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
                            .usage =
                                VkImageUsageFlagBits::
                                    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                            .sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
                            .initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED};
                })
                .requiredImageViewCreateInfo(
                    [](const VkImageCreateInfo &imageCreateInfo,
                       const VkImage &image) noexcept -> VkImageViewCreateInfo {
                        return {.sType = sType<VkImageViewCreateInfo>(),
                                .image = image,
                                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                .format = imageCreateInfo.format,
                                .subresourceRange = {
                                    .aspectMask =
                                        VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1}};
                    }),
            // 3. deep_image
            make_deep_image<context_wsi<surface>>{context_with_surface}
                .setSurfaceExtent(context_with_surface.surfaceExtent())
                .requiredFormatProperties([](VkFormatProperties props) noexcept -> bool {
                    constexpr auto REQUIRED_TILING =
                        VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
                    constexpr auto REQUIRED_FEATURES = VkFormatFeatureFlagBits::
                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
                    return make_deep_image<surface>::selectDeepFormat(
                        props, REQUIRED_TILING, REQUIRED_FEATURES);
                })
                .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                .requiredImageCreateInfo([](make_deep_image<context_wsi<surface>> *self)
                                             -> VkImageCreateInfo {
                    // NOTE: 得到实时窗口大小，非常关键
                    const VkExtent2D &swapChainExtent = self->ref_surfaceExtent();
                    VkFormat depthFormat =
                        self->findDepthFormat({VkFormat::VK_FORMAT_D32_SFLOAT,
                                               VkFormat::VK_FORMAT_D32_SFLOAT_S8_UINT,
                                               VkFormat::VK_FORMAT_D24_UNORM_S8_UINT});

                    return {.sType = sType<VkImageCreateInfo>(),
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = depthFormat,
                            .extent = {.width = swapChainExtent.width,
                                       .height = swapChainExtent.height,
                                       .depth = 1},
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = self->context()->getMaxUsableSampleCount(),
                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage = VkImageUsageFlagBits::
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            .sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
                            .initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED};
                })
                .requiredImageViewCreateInfo(
                    [](const VkImageCreateInfo &imageCreateInfo,
                       const VkImage &image) noexcept -> VkImageViewCreateInfo {
                        return {
                            .sType = sType<VkImageViewCreateInfo>(),
                            .image = image,
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = imageCreateInfo.format,
                            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 .baseMipLevel = 0,
                                                 .levelCount = 1,
                                                 .baseArrayLayer = 0,
                                                 .layerCount = 1}};
                    })};

        // NOTE: 4. descriptor_resource -----------------------------------------------
        const auto MAX_SWAP_CHAIN_IMAGES =
            my_swapchain.ref_swapChain().ref_swapChainImages().size();
        constexpr auto MAX_OBJECTS = 2;

        const auto DESCRIPTOR_COUNT = MAX_OBJECTS * MAX_SWAP_CHAIN_IMAGES;
        auto descriptor_resource =
            make_descriptor_resource<context_wsi<surface>>{context_with_surface}
                .requiredPoolSizes(std::vector{
                    VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         .descriptorCount =
                                             static_cast<uint32_t>(DESCRIPTOR_COUNT)},
                    VkDescriptorPoolSize{
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = static_cast<uint32_t>(DESCRIPTOR_COUNT)}})
                .requiredDescriptorPoolCreateInfo(
                    [](make_descriptor_resource<context_wsi<surface>> *self)
                        -> VkDescriptorPoolCreateInfo {
                        const auto &poolSizes = self->ref_poolSizes();
                        return {.sType = sType<VkDescriptorPoolCreateInfo>(),
                                .flags =
                                    VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                .maxSets = poolSizes[0].descriptorCount,
                                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                .pPoolSizes = poolSizes.data()};
                    })
                .requiredLayoutBinding(std::vector{
                    VkDescriptorSetLayoutBinding{
                        .binding = 0,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = 1,
                        .stageFlags =
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        .pImmutableSamplers = nullptr},
                    VkDescriptorSetLayoutBinding{
                        .binding = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .pImmutableSamplers = nullptr}})
                .requiredDescriptorSetLayoutCreateInfoFn(
                    [](make_descriptor_resource<context_wsi<surface>> *self)
                        -> VkDescriptorSetLayoutCreateInfo {
                        const auto &bindings = self->ref_layoutBinding();
                        return {.sType = sType<VkDescriptorSetLayoutCreateInfo>(),
                                .bindingCount = static_cast<uint32_t>(bindings.size()),
                                .pBindings = bindings.data()};
                    })
                .build();

        // NOTE: 5. GraphicsPipeline: 和呈现有关 参数应该都封装在 my_swapchain 为好
        my_graphics_pipeline graphics_pipeline =
            createGraphicsPipeline(ctx, descriptor_resource, VK_FORMAT_B8G8R8A8_SRGB,
                                   my_swapchain.ref_deepImage().getDeepFormat(),
                                   ctx.ref_physical_device().getMaxUsableSampleCount());

        // NOTE: 6.
        frame_context frameContext{
            ctx, my_swapchain.ref_swapChain().ref_swapChainImages().size()};

        // NOTE: 7. render_object
        const std::string TEXTURE_PATH = "textures/texture.jpg"; // 生产纹理
        const size_t SWAP_CHAIN_SIZE = MAX_SWAP_CHAIN_IMAGES;

        // ==================== 测试场景1：两个重叠的矩形 ====================
        // 创建一个大矩形（蓝色）和一个小矩形（白色），小矩形在大矩形中心重叠
        // 大矩形顶点 (蓝色) // NOLINTBEGIN
        constexpr glm::vec3 blue_color = {0.0F, 0.0F, 1.0F};
        constexpr glm::vec3 white_color = {1.0F, 1.0F, 1.0F};
        std::vector<vertex> bigRectVertices = {
            // 位置 (X, Y, Z), 颜色 (R, G, B), 纹理坐标 (U, V)
            {{-0.5f, 0.5f, 0.0f}, blue_color, {0.0f, 1.0f}},  // 左上 - 蓝色
            {{0.5f, 0.5f, 0.0f}, blue_color, {1.0f, 1.0f}},   // 右上 - 蓝色
            {{0.5f, -0.5f, 0.0f}, blue_color, {1.0f, 0.0f}},  // 右下 - 蓝色
            {{-0.5f, -0.5f, 0.0f}, blue_color, {0.0f, 0.0f}}, // 左下 - 蓝色
        };

        std::vector<uint32_t> bigRectIndices = {0, 1, 2, 2, 3, 0};
        // 小矩形顶点 (白色) - 位于大矩形中心
        std::vector<vertex> smallRectVertices = {
            {{-0.25f, 0.25f, 0.1f}, white_color, {0.0f, 1.0f}},  // 左上
            {{0.25f, 0.25f, 0.1f}, white_color, {1.0f, 1.0f}},   // 右上
            {{0.25f, -0.25f, 0.1f}, white_color, {1.0f, 0.0f}},  // 右下
            {{-0.25f, -0.25f, 0.1f}, white_color, {0.0f, 0.0f}}, // 左下
        };
        std::vector<uint32_t> smallRectIndices = {0, 1, 2, 2, 3, 0};
        // NOLINTEND

        mesh_data mesh0{ctx, bigRectVertices, bigRectIndices};
        mesh_data mesh1{ctx, smallRectVertices, smallRectIndices};

        uniform_buffer uniform_buffer0{ctx, SWAP_CHAIN_SIZE};
        uniform_buffer uniform_buffer1{ctx, SWAP_CHAIN_SIZE};

        auto texture_image_build = std::move(
            make_texture_image<context_wsi<surface>>{context_with_surface}
                .setFilePath(TEXTURE_PATH)
                .setFlip(false)
                .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                .requiredImageCreateInfo([](make_texture_image<context_wsi<surface>>
                                                *self) -> VkImageCreateInfo {
                    const auto &raw_image = self->rawImage();
                    return {
                        .sType = sType<VkImageCreateInfo>(),
                        .imageType = VK_IMAGE_TYPE_2D,
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .extent = {.width = static_cast<uint32_t>(raw_image.width()),
                                   .height = static_cast<uint32_t>(raw_image.height()),
                                   .depth = 1},
                        .mipLevels = raw_image.mipLevels(), // c14: 应用mipLevels
                        .arrayLayers = 1,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .tiling = VK_IMAGE_TILING_OPTIMAL,
                        // c14: 添加VK_IMAGE_USAGE_TRANSFER_SRC_BIT 用于mipmap生成
                        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT,
                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
                    ;
                })
                .requiredImageViewCreateInfo(
                    [](const VkImageCreateInfo &imageCreateInfo,
                       const VkImage &image) noexcept -> VkImageViewCreateInfo {
                        return {
                            .sType = sType<VkImageViewCreateInfo>(),
                            .image = image,
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = imageCreateInfo.format,
                            .subresourceRange = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0,
                                .levelCount = imageCreateInfo.mipLevels, // c14:mipLevels
                                .baseArrayLayer = 0,
                                .layerCount = 1}};
                    })
                .requiredSamplerCreateInfo(
                    [](make_texture_image<context_wsi<surface>> *self)
                        -> VkSamplerCreateInfo {
                        VkPhysicalDeviceProperties properties =
                            self->context()->ref_physical_device().getProperties();

                        return {.sType = sType<VkSamplerCreateInfo>(),
                                .magFilter = VkFilter::VK_FILTER_LINEAR,
                                .minFilter = VkFilter::VK_FILTER_LINEAR,
                                .mipmapMode =
                                    VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                .addressModeU =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .addressModeV =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .addressModeW =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .mipLodBias = 0.0F,
                                // NOTE: 4. 各向异性器件特性启用
                                .anisotropyEnable = VK_TRUE,
                                .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                                .compareEnable = VK_FALSE,
                                .compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS,
                                .minLod = 0.0F,
                                .maxLod = 0.0F,
                                .borderColor = {},
                                .unnormalizedCoordinates = VK_FALSE};
                    }));

        std::vector<VkDescriptorSet> descriptorSets0 =
            graphics_pipeline.ref_descriptorResource().allocateDescriptorSets(
                SWAP_CHAIN_SIZE);
        std::vector<VkDescriptorSet> descriptorSets1 =
            graphics_pipeline.ref_descriptorResource().allocateDescriptorSets(
                SWAP_CHAIN_SIZE);

        std::vector<render_object> objects;
        objects.emplace_back(std::move(mesh0), std::move(uniform_buffer0),
                             texture_image_build.build(), std::move(descriptorSets0));
        objects.emplace_back(std::move(mesh1), std::move(uniform_buffer1),
                             texture_image_build.build(), std::move(descriptorSets1));

        // NOTE: 8. render
        while (window.shouldClose() == 0)
        {
            // objects[0].ref_mesh().update(); // NOTE: 修改顶点或索引不简单，报错
            surface::pollEvents();
            my_render::drawFrame(frameContext, graphics_pipeline, my_swapchain, objects);
        }
        // NOTE: 9. Don't release anything until the GPU is completely idle.
        ::vkDeviceWaitIdle(ctx.raw_logical_device());
        // window.teardown(); //NOTE: auto do that.
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "main done\n";
    return 0;
}