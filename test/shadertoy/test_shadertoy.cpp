// main.cpp - 已集成Shadertoy渲染器
#include "./head.hpp"

#include "shadertoy_renderer.hpp"
#include "shadertoy_vertex.hpp"
#include "shadertoy_uniforms.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <print>
#include <utility>

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
// using mcs::vulkan::make_texture_image;

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

//-------------Shadertoy专用pipeline-------------------------
using mcs::vulkan::descriptor_resource;
using mcs::vulkan::context_base;
using mcs::vulkan::shader_module;

struct my_shadertoy_pipeline
{

    [[nodiscard]] bool valid() const noexcept
    {
        return descriptorResource_.valid() && pipelineLayout_ != nullptr &&
               graphicsPipeline_ != nullptr;
    }

    constexpr my_shadertoy_pipeline(const logical_device &device,
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

    constexpr ~my_shadertoy_pipeline() noexcept
    {
        destroy();
    }
    constexpr my_shadertoy_pipeline(my_shadertoy_pipeline &&o) noexcept
        : device_{std::exchange(o.device_, {})},
          descriptorResource_{std::move(o.descriptorResource_)},
          pipelineLayout_{std::exchange(o.pipelineLayout_, {})},
          graphicsPipeline_{std::exchange(o.graphicsPipeline_, {})}
    {
    }
    constexpr my_shadertoy_pipeline &operator=(my_shadertoy_pipeline &&o) noexcept
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

    my_shadertoy_pipeline(const my_shadertoy_pipeline &) = delete;
    my_shadertoy_pipeline &operator=(const my_shadertoy_pipeline &) = delete;

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

// NOLINTBEGIN

static constexpr my_shadertoy_pipeline createShadertoyPipeline(
    context_base &ctx, descriptor_resource &descriptor_resource, VkFormat colorFormat,
    VkFormat depthFormat, VkSampleCountFlagBits msaaSamples)
{
    const auto &device = ctx.ref_logical_device();
    auto *descriptorSetLayout = descriptor_resource.descriptorSetLayout();

    // 加载Shadertoy着色器（需要预先编译为SPIR-V）
    // NOTE: 只需要修改 frag_file 。 顶点的就算了
    constexpr auto frag_file = "shaders/shadertoy_frag.spv";

    constexpr auto vert_file = "shaders/shadertoy_vert.spv";
    shader_module vert = shader_module(device, vert_file);
    shader_module frag = shader_module(device, frag_file);
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = sType<VkPipelineShaderStageCreateInfo>(),
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert.raw_data(),
        .pName = "main"};
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = sType<VkPipelineShaderStageCreateInfo>(),
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag.raw_data(),
        .pName = "main"};

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    // Shadertoy顶点输入
    auto bindingDescription = shadertoy_vertex::getBindingDescription();
    auto attributeDescriptions = shadertoy_vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
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
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0F};

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
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

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = VK_FALSE, // Shadertoy全屏不需要深度测试
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE};

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = sType<VkPipelineDynamicStateCreateInfo>(),
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = sType<VkPipelineLayoutCreateInfo>(),
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout};

    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline graphicsPipeline = nullptr;
    try
    {
        pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo, nullptr);

        mcs::vulkan::structure_chain<VkGraphicsPipelineCreateInfo,
                                     VkPipelineRenderingCreateInfo>
            pipelineCreateInfoChain = {
                {.stageCount = 2,
                 .pStages = shaderStages,
                 .pVertexInputState = &vertexInputInfo,
                 .pInputAssemblyState = &inputAssembly,
                 .pViewportState = &viewportState,
                 .pRasterizationState = &rasterizer,
                 .pMultisampleState = &multisampling,
                 .pDepthStencilState = &depthStencil,
                 .pColorBlendState = &colorBlending,
                 .pDynamicState = &dynamicState,
                 .layout = pipelineLayout,
                 .renderPass = VK_NULL_HANDLE},
                {.colorAttachmentCount = 1,
                 .pColorAttachmentFormats = &colorFormat,
                 .depthAttachmentFormat = VK_FORMAT_UNDEFINED}}; // 不需要深度附件

        graphicsPipeline = device.createGraphicsPipelines(
            nullptr, 1, pipelineCreateInfoChain.head(), nullptr);

        return my_shadertoy_pipeline{device, std::move(descriptor_resource),
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
// NOLINTEND
struct frame_context
{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    // NOLINTBEGIN
    const logical_device *device_{};
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> presentCompleteSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::vector<VkFence> inFlightFences;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0; // NOLINTEND

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
            device_ = nullptr;
        }
    }
};

// Shadertoy全屏四边形网格
struct shadertoy_mesh_data
{
    // NOLINTBEGIN
    const context_base *context;
    std::vector<shadertoy_vertex> vertices;
    std::vector<uint32_t> indices;
    mcs::vulkan::buffer_base vertexBuffer;
    mcs::vulkan::buffer_base indexBuffer;

    explicit shadertoy_mesh_data(context_base &context) : context{&context}
    {
        // 创建全屏四边形顶点数据
        vertices = {
            {{-1.0f, -1.0f}, {0.0f, 0.0f}}, // 左下
            {{1.0f, -1.0f}, {1.0f, 0.0f}},  // 右下
            {{1.0f, 1.0f}, {1.0f, 1.0f}},   // 右上
            {{-1.0f, 1.0f}, {0.0f, 1.0f}}   // 左上
        };

        indices = {0, 1, 2, 2, 3, 0};

        setup();
    } // NOLINTEND

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

        const VkDeviceSize BUFFER_SIZE = sizeof(shadertoy_vertex) * vertices.size();

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

        syncStatus(vertexBuffer, vertices.data(), BUFFER_SIZE);
    }

    void createIndexBuffer()
    {
        const auto &device = context->ref_logical_device();
        const auto &physicalDevice = context->ref_physical_device();

        const VkDeviceSize BUFFER_SIZE = sizeof(uint32_t) * indices.size();
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
        syncStatus(indexBuffer, indices.data(), BUFFER_SIZE);
    }

  public:
    void bind(VkCommandBuffer commandBuffer) const noexcept
    {
        auto *vertex = vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0}; // NOLINT
        ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, offsets);
        ::vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer(), 0,
                               VK_INDEX_TYPE_UINT32);
    }

    void draw(VkCommandBuffer commandBuffer) const noexcept
    {
        ::vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0,
                           0);
    }
};

// Shadertoy专用Uniform缓冲区
struct shadertoy_uniform_buffer
{
    std::vector<mcs::vulkan::buffer_base> buffers; // NOLINT
    std::vector<void *> mapped;                    // NOLINT

    constexpr shadertoy_uniform_buffer(context_base &ctx, size_t swapChainSize)
    {
        const auto &device = ctx.ref_logical_device();
        const auto &physicalDevice = ctx.ref_physical_device();

        buffers.resize(swapChainSize);
        mapped.resize(swapChainSize);

        const VkDeviceSize BUFFER_SIZE = sizeof(shadertoy_uniforms);
        const VkBufferCreateInfo CREATE_INFO = {
            .sType = sType<VkBufferCreateInfo>(),
            .size = BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        const VkMemoryPropertyFlags PROPERTIES =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        for (size_t i = 0; i < swapChainSize; i++)
        {
            buffers[i] = mcs::vulkan::create_buffer(physicalDevice, device, CREATE_INFO,
                                                    PROPERTIES);
            ::vkMapMemory(device.raw_data(), buffers[i].bufferMemory(), 0, BUFFER_SIZE, 0,
                          &mapped[i]);
        }
    }
    constexpr ~shadertoy_uniform_buffer() noexcept
    {
        destroy();
    }

    shadertoy_uniform_buffer(shadertoy_uniform_buffer &&o) noexcept
        : buffers{std::move(o.buffers)}, mapped{std::move(o.mapped)}
    {
    }

    shadertoy_uniform_buffer &operator=(shadertoy_uniform_buffer &&o) noexcept
    {
        if (&o != this)
        {
            this->destroy();
            buffers = std::move(o.buffers);
            mapped = std::move(o.mapped);
        }
        return *this;
    }
    shadertoy_uniform_buffer(const shadertoy_uniform_buffer &) = delete;
    shadertoy_uniform_buffer &operator=(const shadertoy_uniform_buffer &) = delete;

    void update(uint32_t currentImage, const shadertoy_uniforms &uniforms) const
    {
        ::memcpy(mapped[currentImage], &uniforms, sizeof(shadertoy_uniforms));
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

// Shadertoy渲染对象
struct shadertoy_render_object
{
    // NOLINTBEGIN
    shadertoy_mesh_data mesh;
    shadertoy_uniform_buffer uniformBuffer;
    std::vector<VkDescriptorSet> descriptorSets; // NOLINTEND

    shadertoy_render_object(context_base &ctx, size_t swapChainSize,
                            const descriptor_resource &descriptorResource)
        : mesh(ctx), uniformBuffer(ctx, swapChainSize)
    {
        descriptorSets = descriptorResource.allocateDescriptorSets(swapChainSize);
        updateDescriptorSets(ctx);
    }

    void draw(VkCommandBuffer cmd, uint32_t currentImage,
              VkPipelineLayout pipelineLayout) const
    {
        ::vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                                  1, &descriptorSets[currentImage], 0, nullptr);
        mesh.bind(cmd);
        mesh.draw(cmd);
    }

    void update(uint32_t currentImage, const shadertoy_uniforms &uniforms) const
    {
        uniformBuffer.update(currentImage, uniforms);
    }

  private:
    void updateDescriptorSets(context_base &ctx)
    {
        const auto &device = ctx.ref_logical_device();

        for (size_t i = 0; i < descriptorSets.size(); i++)
        {
            VkDescriptorBufferInfo bufferInfo = {.buffer =
                                                     uniformBuffer.buffers[i].buffer(),
                                                 .offset = 0,
                                                 .range = sizeof(shadertoy_uniforms)};

            VkWriteDescriptorSet descriptorWrite = {
                .sType = sType<VkWriteDescriptorSet>(),
                .dstSet = descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &bufferInfo};

            device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
        }
    }
};

struct my_shadertoy_render
{
    // NOLINTBEGIN
    static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                      VkImageLayout oldLayout, VkImageLayout newLayout,
                                      VkAccessFlags srcAccessMask,
                                      VkAccessFlags dstAccessMask,
                                      VkPipelineStageFlags srcStageMask,
                                      VkPipelineStageFlags dstStageMask,
                                      VkImageAspectFlags aspectMask) // NOLINTEND
    {
        VkImageMemoryBarrier barrier = {.sType = sType<VkImageMemoryBarrier>(),
                                        .srcAccessMask = srcAccessMask,
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
        ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr,
                               0, nullptr, 1, &barrier);
    }

    static void recordCommandBuffer(my_shadertoy_pipeline &pipeline,
                                    my_swap_chain<context_wsi<surface>> &my_swapchain,
                                    shadertoy_render_object &object,
                                    VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                    const VkExtent2D &swapChainExtent)
    {

        const auto &swapChainImages = my_swapchain.ref_swapChain().ref_swapChainImages();
        const auto &swapChainImageViews =
            my_swapchain.ref_swapChain().ref_swapChainImageViews();

        auto *colorImage = my_swapchain.ref_colorImage().image();
        auto *colorImageView = my_swapchain.ref_colorImage().imageView();

        VkCommandBufferBeginInfo beginInfo = {.sType = sType<VkCommandBufferBeginInfo>()};

        if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // ========== 修复点1：添加交换链图像的布局转换（核心遗漏步骤） ==========
        transitionImageLayout(
            commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0, // 从UNDEFINED转换时，源访问掩码为0
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        // ========== 修复点2：修正MSAA颜色图像的转换参数 ==========
        transitionImageLayout(
            commandBuffer, colorImage, VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
            VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0, // 源访问掩码改为0（UNDEFINED布局无前置访问）
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // 源阶段改为TOP_OF_PIPE
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};

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

        VkRenderingInfo renderingInfo = {
            .sType = sType<VkRenderingInfo>(),
            .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
            .pDepthAttachment = nullptr};

        ::vkCmdBeginRendering(commandBuffer, &renderingInfo);

        ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.graphicsPipeline());

        VkViewport viewport = {.x = 0.0F,
                               .y = 0.0F,
                               .width = static_cast<float>(swapChainExtent.width),
                               .height = static_cast<float>(swapChainExtent.height),
                               .minDepth = 0.0F,
                               .maxDepth = 1.0F};
        ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
        ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        object.draw(commandBuffer, imageIndex, pipeline.pipelineLayout());

        ::vkCmdEndRendering(commandBuffer);

        // Transition image layout for presentation（这部分保留，是正确的）
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

    static void drawFrame(frame_context &frame_context, my_shadertoy_pipeline &pipeline,
                          my_swap_chain<context_wsi<surface>> &my_swapchain,
                          shadertoy_render_object &object,
                          const shadertoy_uniforms &uniforms)
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

        // 更新Uniform数据
        object.update(imageIndex, uniforms);

        recordCommandBuffer(pipeline, my_swapchain, object, commandBuffers[currentFrame],
                            imageIndex, my_swapchain.ref_surfaceExtent());

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

// NOLINTBEGIN
using mcs::vulkan::event::mousebutton_event_dispatcher;
using mcs::vulkan::event::mousebutton_event;
using mcs::vulkan::event::cursor_pos_event_dispatcher;
using mcs::vulkan::event::position2d_event;

static std::map<uint64_t, std::function<void(mousebutton_event)>> mousebutton_fn;
static std::map<uint64_t, std::function<void(position2d_event)>> cursorPos_fn;
struct even_reciver
{
    static void mousebutton_event(mousebutton_event mouse) noexcept
    {
        for (auto &[id, fn] : mousebutton_fn)
        {
            std::invoke(fn, mouse);
        }
    }
    static void cursorPos_event(position2d_event mouse) noexcept
    {
        for (auto &[id, fn] : cursorPos_fn)
        {
            std::invoke(fn, mouse);
        }
    }
};
// NOLINTEND

int main()
{
    try
    {
        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan11Features,
                        VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                {.features = {.samplerAnisotropy = VK_TRUE}},
                {.shaderDrawParameters = VK_TRUE},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

        // 创建窗口
        surface window{};
        window.setup({.width = 800, .height = 600}, "Shadertoy Renderer"); // NOLINT

        // 创建Vulkan上下文
        mcs::vulkan::context_base ctx;
        ctx.createInstance(make_instance{}
                               .enableDebugExtension()
                               .enableSurfaceExtension<surface>()
                               .checkExtensionSupport()
                               .checkLayerSupport()
                               .build({.sType = sType<VkApplicationInfo>(),
                                       .pApplicationName = "Shadertoy Renderer",
                                       .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                       .pEngineName = "No Engine",
                                       .engineVersion = VK_MAKE_VERSION(1, 0, 0),
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
                    .afterQueueCreateInfoInit(
                        [](VkDeviceQueueCreateInfo &queueCreateInfo) noexcept {})
                    .afterBuildSuccess([&](const logical_device &logicalDevice,
                                           uint32_t queueFamilyIndex) {
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

        // 创建带表面的上下文
        context_wsi<surface> context_with_surface{ctx, window};

        // 创建交换链
        my_swap_chain<context_wsi<surface>> my_swapchain = {
            make_swap_chain<context_wsi<surface>>{context_with_surface}
                .setSurfaceExtent(context_with_surface.surfaceExtent())
                .requiredSwapchainCreateInfoKHR(
                    [](make_swap_chain<context_wsi<surface>> *self)
                        -> VkSwapchainCreateInfoKHR {
                        auto *ctx = self->context();
                        const VkExtent2D &swapChainExtent = self->ref_surfaceExtent();

                        auto swapChainSurfaceFormat = ctx->chooseSurfaceFormat(
                            {.format = VK_FORMAT_B8G8R8A8_SRGB,
                             .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

                        auto minImageCount = ctx->minImageCount();
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
            make_color_image<context_wsi<surface>>{context_with_surface}
                .setSurfaceExtent(context_with_surface.surfaceExtent())
                .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                .requiredImageCreateInfo([](make_color_image<context_wsi<surface>> *self)
                                             -> VkImageCreateInfo {
                    const VkExtent2D &swapChainExtent = self->ref_surfaceExtent();
                    auto swapChainSurfaceFormat = self->context()->chooseSurfaceFormat(
                        {.format = VK_FORMAT_B8G8R8A8_SRGB,
                         .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
                    auto msaaSamples = self->context()->getMaxUsableSampleCount();
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

        // 创建描述符资源（只需要Uniform缓冲区）
        const auto MAX_SWAP_CHAIN_IMAGES =
            my_swapchain.ref_swapChain().ref_swapChainImages().size();
        constexpr auto DESCRIPTOR_COUNT = 1;

        auto descriptor_resource =
            make_descriptor_resource<context_wsi<surface>>{context_with_surface}
                .requiredPoolSizes(std::vector{VkDescriptorPoolSize{
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount =
                        static_cast<uint32_t>(DESCRIPTOR_COUNT * MAX_SWAP_CHAIN_IMAGES)}})
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
                .requiredLayoutBinding(std::vector{VkDescriptorSetLayoutBinding{
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags =
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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

        // 创建Shadertoy渲染管道
        my_shadertoy_pipeline shadertoy_pipeline = createShadertoyPipeline(
            ctx, descriptor_resource, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_UNDEFINED,
            ctx.ref_physical_device().getMaxUsableSampleCount());

        // 创建帧上下文
        frame_context frameContext{
            ctx, my_swapchain.ref_swapChain().ref_swapChainImages().size()};

        // 创建Shadertoy渲染对象
        shadertoy_render_object shadertoy_object(
            ctx, MAX_SWAP_CHAIN_IMAGES, shadertoy_pipeline.ref_descriptorResource());

        // 创建Shadertoy渲染器
        ShadertoyRenderer shadertoyRenderer;

        // 设置事件回调
        mousebutton_event_dispatcher::instance().subscribe(
            &even_reciver::mousebutton_event);
        cursor_pos_event_dispatcher::instance().subscribe(&even_reciver::cursorPos_event);

        auto mouseButtonCallback = [&](mousebutton_event event) {
            bool pressed = (event.action == mcs::vulkan::event::Action::PRESS);
            shadertoyRenderer.onMouseButton(pressed);
            std::println("Mouse button pressed: {}", pressed);
        };
        // NOLINTNEXTLINE
        mousebutton_fn.emplace(uint64_t(&mouseButtonCallback),
                               std::move(mouseButtonCallback));

        auto mouseMoveCallback = [&](cursor_pos_event_dispatcher::event_type event) {
            shadertoyRenderer.onMouseMove(static_cast<float>(event.xpos),
                                          static_cast<float>(event.ypos));
        }; // NOLINTNEXTLINE
        cursorPos_fn.emplace(uint64_t(&mouseMoveCallback), std::move(mouseMoveCallback));

        // 主渲染循环
        while (window.shouldClose() == 0)
        {
            surface::pollEvents();

            // 获取窗口大小并更新Uniforms
            auto extent = window.getFramebufferSize();
            shadertoyRenderer.updateUniforms(extent.width, extent.height);

            // 渲染帧
            my_shadertoy_render::drawFrame(frameContext, shadertoy_pipeline, my_swapchain,
                                           shadertoy_object,
                                           shadertoyRenderer.getUniforms());
        }

        // 等待GPU空闲
        ::vkDeviceWaitIdle(ctx.raw_logical_device());
    }
    catch (std::exception &e)
    {
        std::println("Error: {}", e.what());
    }

    std::cout << "Shadertoy renderer terminated\n";
    return 0;
}