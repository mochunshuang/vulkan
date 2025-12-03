
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

import std;
import std.compat;

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

import mcs_vulkan;

// NOLINTBEGIN

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return {vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat,
                                                    offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat,
                                                    offsetof(Vertex, color)),
                vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat,
                                                    offsetof(Vertex, texCoord))};
    }
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// 为不同图元拓扑创建不同的顶点数据
const std::vector<Vertex> point_vertices = {
    {{-0.8f, -0.8f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 红色 - 左下
    {{0.8f, -0.8f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // 绿色 - 右下
    {{0.0f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},   // 蓝色 - 上
    {{-0.4f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.3f, 0.5f}},  // 黄色 - 左中
    {{0.4f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.7f, 0.5f}}    // 紫色 - 右中
};

const std::vector<uint16_t> point_indices = {0, 1, 2, 3, 4};

const std::vector<Vertex> line_vertices = {
    {{-0.9f, -0.9f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 左下 - 红
    {{0.9f, -0.9f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // 右下 - 绿
    {{0.9f, 0.9f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},   // 右上 - 蓝
    {{-0.9f, 0.9f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},  // 左上 - 黄
    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.5f, 0.5f}}    // 中心 - 紫
};

const std::vector<uint16_t> line_list_indices = {0, 1, 1, 2, 2, 3, 3, 0,
                                                 0, 4, 1, 4, 2, 4, 3, 4};
const std::vector<uint16_t> line_strip_indices = {0, 1, 2, 3, 0};

const std::vector<Vertex> triangle_vertices = {
    {{-0.8f, -0.8f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // 左下 - 红
    {{0.8f, -0.8f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},   // 右下 - 绿
    {{0.0f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},    // 上 - 蓝
    {{-0.4f, -0.2f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.25f, 0.4f}}, // 左中 - 黄
    {{0.4f, -0.2f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.75f, 0.4f}}   // 右中 - 紫
};

const std::vector<uint16_t> triangle_list_indices = {0, 1, 2, 0, 3, 4};
const std::vector<uint16_t> triangle_strip_indices = {0, 1, 3, 2, 4};
const std::vector<uint16_t> triangle_fan_indices = {0, 1, 2, 3, 4};

namespace glfw_namespace
{
    struct swapchain;

    struct window
    {
        static constexpr uint32_t WIDTH = 800;
        static constexpr uint32_t HEIGHT = 600;

        window() = default;
        window(const window &) = delete;
        constexpr window(window &&other) noexcept
            : window_{std::exchange(other.window_, nullptr)},
              framebufferResized_{std::exchange(other.framebufferResized_, false)}
        {
        }
        window &operator=(const window &) = delete;
        window &operator=(window &&) = delete;

        void setup_window()
        {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
            window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Primitive Topology Test",
                                       nullptr, nullptr);
            glfwSetWindowUserPointer(window_, this);
            glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
        }

        void cleanup()
        {
            std::destroy_at(&surface_);
            glfwDestroyWindow(window_);
            glfwTerminate();
        }

        [[nodiscard]] constexpr bool is_resized() const noexcept
        {
            return framebufferResized_;
        }
        [[nodiscard]] bool is_close() const noexcept
        {
            return glfwWindowShouldClose(window_);
        }
        void poll_events() noexcept
        {
            glfwPollEvents();
        }

        void wait_for_valid_framebuffer() noexcept
        {
            int width = 0, height = 0;
            glfwGetFramebufferSize(window_, &width, &height);
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(window_, &width, &height);
                glfwWaitEvents();
            }
        }

        auto framebuffer_size() noexcept
        {
            int width, height;
            glfwGetFramebufferSize(window_, &width, &height);
            return std::make_pair(width, height);
        }

        auto &ref_framebuffer_size() noexcept
        {
            return framebufferResized_;
        }

        void setup_surface(vk::raii::Instance &instance)
        {
            VkSurfaceKHR _surface;
            if (glfwCreateWindowSurface(*instance, window_, nullptr, &_surface) != 0)
                throw std::runtime_error("failed to create window surface!");
            surface_ = vk::raii::SurfaceKHR(instance, _surface);
        }

        vk::raii::SurfaceKHR &surface() noexcept
        {
            return surface_;
        }

        static constexpr auto glfw_extensions()
        {
#ifdef NDEBUG
            constexpr bool enableValidationLayers = false;
#else
            constexpr bool enableValidationLayers = true;
#endif
            uint32_t glfwExtensionCount = 0;
            auto *glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
            if (enableValidationLayers)
            {
                extensions.push_back(vk::EXTDebugUtilsExtensionName);
            }
            // extensions.push_back("VK_EXT_extended_dynamic_state");

            return extensions;
        }

        static void init_extensions_to_vulkan()
        {
            mcs::vulkan::vulkan_config::extensions() = glfw_extensions();
        }

      private:
        GLFWwindow *window_{};
        vk::raii::SurfaceKHR surface_ = nullptr;
        bool framebufferResized_{};

        static void framebufferResizeCallback(GLFWwindow *ptr, int /*width*/,
                                              int /*height*/)
        {
            auto *self = static_cast<window *>(glfwGetWindowUserPointer(ptr));
            self->framebufferResized_ = true;
        }
    };

    struct swapchain
    {
        vk::raii::SwapchainKHR swapChain = nullptr;
        std::vector<vk::Image> swapChainImages;
        vk::SurfaceFormatKHR swapChainSurfaceFormat;
        vk::Extent2D swapChainExtent;
        std::vector<vk::raii::ImageView> swapChainImageViews;
        mcs::vulkan::vulkan_image depth_resource;

      private:
        static uint32_t chooseSwapMinImageCount(
            const vk::SurfaceCapabilitiesKHR &surfaceCapabilities)
        {
            constexpr auto k_min_count = 3u;
            auto minImageCount = std::max(k_min_count, surfaceCapabilities.minImageCount);
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
            const auto k_format_it =
                std::ranges::find_if(availableFormats, [](const auto &format) {
                    return format.format == vk::Format::eB8G8R8A8Srgb &&
                           format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                });
            return k_format_it != availableFormats.end() ? *k_format_it
                                                         : availableFormats[0];
        }

        static vk::PresentModeKHR chooseSwapPresentMode(
            const std::vector<vk::PresentModeKHR> &availablePresentModes)
        {
            assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {
                return presentMode == vk::PresentModeKHR::eFifo;
            }));
            return std::ranges::any_of(availablePresentModes,
                                       [](const vk::PresentModeKHR &value) {
                                           return vk::PresentModeKHR::eMailbox == value;
                                       })
                       ? vk::PresentModeKHR::eMailbox
                       : vk::PresentModeKHR::eFifo;
        }

        static vk::Extent2D chooseSwapExtent(
            const vk::SurfaceCapabilitiesKHR &capabilities, window &window)
        {
            if (capabilities.currentExtent.width != 0xFFFFFFFF)
            {
                return capabilities.currentExtent;
            }
            auto [width, height] = window.framebuffer_size();
            return {
                .width = std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                              capabilities.maxImageExtent.width),
                .height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                               capabilities.maxImageExtent.height)};
        }

        void createImageViews(vk::raii::Device &device)
        {
            assert(swapChainImageViews.empty());
            vk::ImageViewCreateInfo imageViewCreateInfo{
                .viewType = vk::ImageViewType::e2D,
                .format = swapChainSurfaceFormat.format,
                .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            for (auto &image : swapChainImages)
            {
                imageViewCreateInfo.image = image;
                swapChainImageViews.emplace_back(device, imageViewCreateInfo);
            }
        }

        void createSwapChain(::mcs::vulkan::vulkan_device &vulkan_device, window &window)
        {
            auto &physicalDevice = vulkan_device.physicalDevice;
            auto &device = vulkan_device.device;

            auto surfaceCapabilities =
                physicalDevice.getSurfaceCapabilitiesKHR(*window.surface());
            swapChainExtent = chooseSwapExtent(surfaceCapabilities, window);
            swapChainSurfaceFormat = chooseSwapSurfaceFormat(
                physicalDevice.getSurfaceFormatsKHR(*window.surface()));

            vk::SwapchainCreateInfoKHR swapChainCreateInfo{
                .surface = *window.surface(),
                .minImageCount = chooseSwapMinImageCount(surfaceCapabilities),
                .imageFormat = swapChainSurfaceFormat.format,
                .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                .imageExtent = swapChainExtent,
                .imageArrayLayers = 1,
                .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                .imageSharingMode = vk::SharingMode::eExclusive,
                .preTransform = surfaceCapabilities.currentTransform,
                .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                .presentMode = chooseSwapPresentMode(
                    physicalDevice.getSurfacePresentModesKHR(*window.surface())),
                .clipped = true};

            swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
            swapChainImages = swapChain.getImages();
        }

        void cleanupSwapChain()
        {
            swapChainImageViews.clear();
            swapChain = nullptr;
        }

      public:
        void setup_swapchain(::mcs::vulkan::vulkan_device &vulkan_device, window &window)
        {
            createSwapChain(vulkan_device, window);
            createImageViews(vulkan_device.device);
        }

        void recreateSwapChain(::mcs::vulkan::vulkan_device &vulkan_device,
                               window &window)
        {
            auto &device = vulkan_device.device;
            window.wait_for_valid_framebuffer();
            device.waitIdle();
            cleanupSwapChain();
            createSwapChain(vulkan_device, window);
            createImageViews(device);
            createDepthResources(vulkan_device);
        }

        static vk::Format findDepthFormat(::mcs::vulkan::vulkan_device &vulkan_device)
        {
            return vulkan_device.findSupportedFormat(
                {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
                 vk::Format::eD24UnormS8Uint},
                vk::ImageTiling::eOptimal,
                vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        }

        void createDepthResources(::mcs::vulkan::vulkan_device &vulkan_device)
        {
            vk::Format depthFormat = findDepthFormat(vulkan_device);
            auto &[depthImage, depthImageMemory, depthImageView] = depth_resource;
            createImage(vulkan_device, swapChainExtent.width, swapChainExtent.height,
                        depthFormat, vk::ImageTiling::eOptimal,
                        vk::ImageUsageFlagBits::eDepthStencilAttachment,
                        vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage,
                        depthImageMemory);
            depthImageView = vulkan_device.createImageView(
                depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
        }

        void createImage(::mcs::vulkan::vulkan_device &vulkan_device, uint32_t width,
                         uint32_t height, vk::Format format, vk::ImageTiling tiling,
                         vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
                         vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory)
        {
            auto &device = vulkan_device.device;
            vk::ImageCreateInfo imageInfo{
                .imageType = vk::ImageType::e2D,
                .format = format,
                .extent = {.width = width, .height = height, .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = tiling,
                .usage = usage,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined};
            image = vk::raii::Image(device, imageInfo);

            vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
            vk::MemoryAllocateInfo allocInfo{
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = vulkan_device.findMemoryType(
                    memRequirements.memoryTypeBits, properties)};
            imageMemory = vk::raii::DeviceMemory(device, allocInfo);
            image.bindMemory(imageMemory, 0);
        }

        void cleanup()
        {
            depth_resource.cleanup();
            swapChainImageViews.clear();
            swapChain = nullptr;
        }
    };
}; // namespace glfw_namespace

struct graphics_pipeline
{
    using swapchain_type = glfw_namespace::swapchain;

    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;

  private:
    void createDescriptorSetLayout(vk::raii::Device &device)
    {
        std::array bindings = {
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                           vk::ShaderStageFlagBits::eVertex, nullptr),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler,
                                           1, vk::ShaderStageFlagBits::eFragment,
                                           nullptr)};

        vk::DescriptorSetLayoutCreateInfo layoutInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()};
        descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
    }

    void createGraphicsPipeline(::mcs::vulkan::vulkan_device &vulkan_device,
                                swapchain_type &swapchain)
    {
        auto &device = vulkan_device.device;
        auto &swapChainSurfaceFormat = swapchain.swapChainSurfaceFormat;

        vk::raii::ShaderModule shaderModule = vulkan_device.createShaderModule(
            mcs::vulkan::readFile("shaders/27_shader_depth.spv"));

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

        // 关键修复：启用 dynamicPrimitiveTopologyUnrestricted 特性
        // 这将允许我们在不同拓扑类之间动态切换
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::ePointList, // 使用点列表作为基础拓扑
            .primitiveRestartEnable = vk::True};

        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                          .scissorCount = 1};

        // 关键修复：确保线宽为1.0，除非启用了wideLines特性
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0f}; // 修复：使用1.0线宽，避免wideLines特性需求

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False};

        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = vk::True,
            .depthWriteEnable = vk::True,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = vk::False,
            .stencilTestEnable = vk::False};

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = vk::False;

        vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable = vk::False,
                                                            .logicOp = vk::LogicOp::eCopy,
                                                            .attachmentCount = 1,
                                                            .pAttachments =
                                                                &colorBlendAttachment};

        // 动态状态 - 移除线宽动态状态，因为我们使用固定线宽1.0
        std::vector dynamicStates = {vk::DynamicState::eViewport,
                                     vk::DynamicState::eScissor,
                                     vk::DynamicState::ePrimitiveTopology,
                                     vk::DynamicState::ePrimitiveRestartEnable};

        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1,
                                                        .pSetLayouts =
                                                            &*descriptorSetLayout,
                                                        .pushConstantRangeCount = 0};

        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::Format depthFormat = swapchain_type::findDepthFormat(vulkan_device);

        // 将特性添加到管道创建信息链中
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
                 .pDepthStencilState = &depthStencil,
                 .pColorBlendState = &colorBlending,
                 .pDynamicState = &dynamicState,
                 .layout = pipelineLayout,
                 .renderPass = nullptr},
                {.colorAttachmentCount = 1,
                 .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
                 .depthAttachmentFormat = depthFormat}};

        graphicsPipeline = vk::raii::Pipeline(
            device, nullptr,
            pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    }

  public:
    void setup_pipeline(::mcs::vulkan::vulkan_device &vulkan_device,
                        swapchain_type &swapchain)
    {
        createDescriptorSetLayout(vulkan_device.device);
        createGraphicsPipeline(vulkan_device, swapchain);
    }

    void cleanup()
    {
        graphicsPipeline = nullptr;
        pipelineLayout = nullptr;
        descriptorSetLayout = nullptr;
    }
};

struct vulkan_texture
{
    using swapchain_type = glfw_namespace::swapchain;
    mcs::vulkan::vulkan_image texture;
    vk::raii::Sampler textureSampler = nullptr;

  private:
    std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands(
        vk::raii::CommandPool &commandPool, vk::raii::Device &device)
    {
        vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1};
        std::unique_ptr<vk::raii::CommandBuffer> commandBuffer =
            std::make_unique<vk::raii::CommandBuffer>(
                std::move(vk::raii::CommandBuffers(device, allocInfo).front()));
        vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
        commandBuffer->begin(beginInfo);
        return commandBuffer;
    }

    void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer,
                               vk::raii::Queue &queue)
    {
        commandBuffer.end();
        vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                                  .pCommandBuffers = &*commandBuffer};
        queue.submit(submitInfo, nullptr);
        queue.waitIdle();
    }

    void transitionImageLayout(::mcs::vulkan::vulkan_device &vulkan_device,
                               const vk::raii::Image &image, vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout)
    {
        auto &commandPool = vulkan_device.commandPool;
        auto &device = vulkan_device.device;
        vk::raii::Queue &queue = vulkan_device.queue;

        auto commandBuffer = beginSingleTimeCommands(commandPool, device);
        vk::ImageMemoryBarrier barrier{
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .image = image,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};

        vk::PipelineStageFlags sourceStage;
        vk::PipelineStageFlags destinationStage;

        if (oldLayout == vk::ImageLayout::eUndefined &&
            newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
                 newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }
        commandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr,
                                       barrier);
        endSingleTimeCommands(*commandBuffer, queue);
    }

    void copyBufferToImage(::mcs::vulkan::vulkan_device &vulkan_device,
                           const vk::raii::Buffer &buffer, vk::raii::Image &image,
                           uint32_t width, uint32_t height)
    {
        auto &commandPool = vulkan_device.commandPool;
        auto &device = vulkan_device.device;
        vk::raii::Queue &queue = vulkan_device.queue;

        std::unique_ptr<vk::raii::CommandBuffer> commandBuffer =
            beginSingleTimeCommands(commandPool, device);
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .mipLevel = 0,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1},
            .imageOffset = {.x = 0, .y = 0, .z = 0},
            .imageExtent = {.width = width, .height = height, .depth = 1}};
        commandBuffer->copyBufferToImage(buffer, image,
                                         vk::ImageLayout::eTransferDstOptimal, {region});
        endSingleTimeCommands(*commandBuffer, queue);
    }

  public:
    void createTextureImage(::mcs::vulkan::vulkan_device &vulkan_device,
                            swapchain_type &swapchain)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight,
                                    &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels)
            throw std::runtime_error("failed to load texture image!");

        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vulkan_device.createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                   stagingBuffer, stagingBufferMemory);

        void *data = stagingBufferMemory.mapMemory(0, imageSize);
        memcpy(data, pixels, imageSize);
        stagingBufferMemory.unmapMemory();
        stbi_image_free(pixels);

        auto &[textureImage, textureImageMemory, textureImageView] = texture;
        swapchain.createImage(
            vulkan_device, texWidth, texHeight, vk::Format::eR8G8B8A8Srgb,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);

        transitionImageLayout(vulkan_device, textureImage, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eTransferDstOptimal);
        copyBufferToImage(vulkan_device, stagingBuffer, textureImage,
                          static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight));
        transitionImageLayout(vulkan_device, textureImage,
                              vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    void createTextureImageView(::mcs::vulkan::vulkan_device &vulkan_device)
    {
        auto &[textureImage, textureImageMemory, textureImageView] = texture;
        textureImageView = vulkan_device.createImageView(
            textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
    }

    void createTextureSampler(::mcs::vulkan::vulkan_device &vulkan_device)
    {
        vk::PhysicalDeviceProperties properties =
            vulkan_device.physicalDevice.getProperties();
        vk::SamplerCreateInfo samplerInfo{.magFilter = vk::Filter::eLinear,
                                          .minFilter = vk::Filter::eLinear,
                                          .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                          .addressModeU = vk::SamplerAddressMode::eRepeat,
                                          .addressModeV = vk::SamplerAddressMode::eRepeat,
                                          .addressModeW = vk::SamplerAddressMode::eRepeat,
                                          .mipLodBias = 0.0f,
                                          .anisotropyEnable = vk::True,
                                          .maxAnisotropy =
                                              properties.limits.maxSamplerAnisotropy,
                                          .compareEnable = vk::False,
                                          .compareOp = vk::CompareOp::eAlways};
        textureSampler = vk::raii::Sampler(vulkan_device.device, samplerInfo);
    }

    void setup_texture(::mcs::vulkan::vulkan_device &vulkan_device,
                       swapchain_type &swapchain)
    {
        createTextureImage(vulkan_device, swapchain);
        createTextureImageView(vulkan_device);
        createTextureSampler(vulkan_device);
    }

    void cleanup()
    {
        textureSampler.clear();
        texture.cleanup();
    }
};

struct vulkan_vertex
{
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;
    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::DeviceMemory indexBufferMemory = nullptr;

    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    // 当前使用的顶点和索引数据
    const std::vector<Vertex> *currentVertices = &point_vertices;
    const std::vector<uint16_t> *currentIndices = &point_indices;
    vk::PrimitiveTopology currentTopology = vk::PrimitiveTopology::ePointList;
    bool primitiveRestartEnable = false;

  public:
    void createVertexBuffer(::mcs::vulkan::vulkan_device &vulkan_device)
    {
        vk::DeviceSize bufferSize =
            sizeof((*currentVertices)[0]) * currentVertices->size();
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vulkan_device.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                   stagingBuffer, stagingBufferMemory);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, currentVertices->data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        vulkan_device.createBuffer(bufferSize,
                                   vk::BufferUsageFlagBits::eTransferDst |
                                       vk::BufferUsageFlagBits::eVertexBuffer,
                                   vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer,
                                   vertexBufferMemory);
        vulkan_device.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    }

    void createIndexBuffer(::mcs::vulkan::vulkan_device &vulkan_device)
    {
        vk::DeviceSize bufferSize = sizeof((*currentIndices)[0]) * currentIndices->size();
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vulkan_device.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                   stagingBuffer, stagingBufferMemory);

        void *data = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, currentIndices->data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        vulkan_device.createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);
        vulkan_device.copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    }

    void updateBuffers(::mcs::vulkan::vulkan_device &vulkan_device)
    {
        // 等待设备空闲，确保缓冲区不再被使用
        vulkan_device.device.waitIdle();

        // 清理旧缓冲区
        indexBuffer = nullptr;
        indexBufferMemory = nullptr;
        vertexBuffer = nullptr;
        vertexBufferMemory = nullptr;

        // 创建新缓冲区
        createVertexBuffer(vulkan_device);
        createIndexBuffer(vulkan_device);
    }

    void createUniformBuffers(::mcs::vulkan::vulkan_device &vulkan_device)
    {
        uniformBuffers.clear();
        uniformBuffersMemory.clear();
        uniformBuffersMapped.clear();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
            vk::raii::Buffer buffer({});
            vk::raii::DeviceMemory bufferMem({});
            vulkan_device.createBuffer(bufferSize,
                                       vk::BufferUsageFlagBits::eUniformBuffer,
                                       vk::MemoryPropertyFlagBits::eHostVisible |
                                           vk::MemoryPropertyFlagBits::eHostCoherent,
                                       buffer, bufferMem);
            uniformBuffers.emplace_back(std::move(buffer));
            uniformBuffersMemory.emplace_back(std::move(bufferMem));
            uniformBuffersMapped.emplace_back(
                uniformBuffersMemory[i].mapMemory(0, bufferSize));
        }
    }

    void createDescriptorPool(vk::raii::Device &device)
    {
        std::array poolSize{
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                                   MAX_FRAMES_IN_FLIGHT),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                                   MAX_FRAMES_IN_FLIGHT)};
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
            .pPoolSizes = poolSize.data()};
        descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
    }

    void createDescriptorSets(graphics_pipeline &pipeline,
                              ::mcs::vulkan::vulkan_device &vulkan_device,
                              vulkan_texture &texture)
    {
        auto &descriptorSetLayout = pipeline.descriptorSetLayout;
        vk::raii::Device &device = vulkan_device.device;
        auto &[image, textureSampler] = texture;
        auto &textureImageView = image.imageView;

        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                     descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool,
                                                .descriptorSetCount =
                                                    static_cast<uint32_t>(layouts.size()),
                                                .pSetLayouts = layouts.data()};

        descriptorSets.clear();
        descriptorSets = device.allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBuffers[i],
                                                .offset = 0,
                                                .range = sizeof(UniformBufferObject)};
            vk::DescriptorImageInfo imageInfo{
                .sampler = textureSampler,
                .imageView = textureImageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
            std::array descriptorWrites{
                vk::WriteDescriptorSet{.dstSet = descriptorSets[i],
                                       .dstBinding = 0,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType =
                                           vk::DescriptorType::eUniformBuffer,
                                       .pBufferInfo = &bufferInfo},
                vk::WriteDescriptorSet{.dstSet = descriptorSets[i],
                                       .dstBinding = 1,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType =
                                           vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &imageInfo}};
            device.updateDescriptorSets(descriptorWrites, {});
        }
    }

    void setup_vertex(graphics_pipeline &pipeline,
                      ::mcs::vulkan::vulkan_device &vulkan_device,
                      vulkan_texture &texture)
    {
        createVertexBuffer(vulkan_device);
        createIndexBuffer(vulkan_device);
        createUniformBuffers(vulkan_device);
        createDescriptorPool(vulkan_device.device);
        createDescriptorSets(pipeline, vulkan_device, texture);
    }

    void setTopology(vk::PrimitiveTopology topology,
                     ::mcs::vulkan::vulkan_device &vulkan_device)
    {
        currentTopology = topology;
        primitiveRestartEnable = (topology == vk::PrimitiveTopology::eTriangleStrip ||
                                  topology == vk::PrimitiveTopology::eLineStrip);

        // 根据拓扑选择不同的顶点和索引数据
        switch (topology)
        {
        case vk::PrimitiveTopology::ePointList:
            currentVertices = &point_vertices;
            currentIndices = &point_indices;
            break;
        case vk::PrimitiveTopology::eLineList:
            currentVertices = &line_vertices;
            currentIndices = &line_list_indices;
            break;
        case vk::PrimitiveTopology::eLineStrip:
            currentVertices = &line_vertices;
            currentIndices = &line_strip_indices;
            break;
        case vk::PrimitiveTopology::eTriangleList:
            currentVertices = &triangle_vertices;
            currentIndices = &triangle_list_indices;
            break;
        case vk::PrimitiveTopology::eTriangleStrip:
            currentVertices = &triangle_vertices;
            currentIndices = &triangle_strip_indices;
            break;
        case vk::PrimitiveTopology::eTriangleFan:
            currentVertices = &triangle_vertices;
            currentIndices = &triangle_fan_indices;
            break;
        default:
            currentVertices = &point_vertices;
            currentIndices = &point_indices;
            break;
        }

        // 更新缓冲区
        updateBuffers(vulkan_device);
    }

    void cleanup()
    {
        descriptorSets.clear();
        descriptorPool.clear();
        uniformBuffersMapped.clear();
        uniformBuffersMemory.clear();
        uniformBuffers.clear();
        indexBuffer.clear();
        indexBufferMemory.clear();
        vertexBuffer.clear();
        vertexBufferMemory.clear();
    }
};

struct drawing
{
    using swapchain_type = glfw_namespace::swapchain;
    using window_type = glfw_namespace::window;

    std::vector<vk::raii::CommandBuffer> commandBuffers;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphore;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphore;
    std::vector<vk::raii::Fence> inFlightFences;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;

    // 拓扑测试相关 - 包含所有支持的拓扑
    std::vector<vk::PrimitiveTopology> topologies = {
        vk::PrimitiveTopology::ePointList,     vk::PrimitiveTopology::eLineList,
        vk::PrimitiveTopology::eLineStrip,     vk::PrimitiveTopology::eTriangleList,
        vk::PrimitiveTopology::eTriangleStrip, vk::PrimitiveTopology::eTriangleFan};
    std::vector<std::string> topologyNames = {"Point List",     "Line List",
                                              "Line Strip",     "Triangle List",
                                              "Triangle Strip", "Triangle Fan"};
    size_t currentTopologyIndex = 0;
    int frameCounter = 0;
    const int framesPerTopology = 300; // 每个拓扑显示300帧（约5秒）

  private:
    void createCommandBuffers(::mcs::vulkan::vulkan_device &vulkan_device)
    {
        commandBuffers.clear();
        auto &commandPool = vulkan_device.commandPool;
        auto &device = vulkan_device.device;
        vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount =
                                                    MAX_FRAMES_IN_FLIGHT};
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    }

    void createSyncObjects(::mcs::vulkan::vulkan_device &vulkan_device,
                           swapchain_type &swapchain)
    {
        auto &device = vulkan_device.device;
        auto &swapChainImages = swapchain.swapChainImages;

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

    void updateUniformBuffer(uint32_t currentImage, vk::Extent2D &swapChainExtent,
                             std::vector<void *> &uniformBuffersMapped)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f),
                                glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                               glm::vec3(0.0f, -1.0f, 0.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f),
                                    static_cast<float>(swapChainExtent.width) /
                                        static_cast<float>(swapChainExtent.height),
                                    0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void transition_image_layout(vk::Image image, vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout,
                                 vk::AccessFlags2 src_access_mask,
                                 vk::AccessFlags2 dst_access_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask,
                                 vk::ImageAspectFlags image_aspect_flags)
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
            .image = image,
            .subresourceRange = {.aspectMask = image_aspect_flags,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};
        vk::DependencyInfo dependency_info = {.dependencyFlags = {},
                                              .imageMemoryBarrierCount = 1,
                                              .pImageMemoryBarriers = &barrier};
        commandBuffers[currentFrame].pipelineBarrier2(dependency_info);
    }

    void recordCommandBuffer(uint32_t imageIndex, swapchain_type &swapchain,
                             vulkan_vertex &vertex, graphics_pipeline &pipeline)
    {
        auto &swapChainImages = swapchain.swapChainImages;
        auto &swapChainImageViews = swapchain.swapChainImageViews;
        auto &swapChainExtent = swapchain.swapChainExtent;
        auto &[depthImage, _, depthImageView] = swapchain.depth_resource;

        auto &vertexBuffer = vertex.vertexBuffer;
        auto &indexBuffer = vertex.indexBuffer;
        auto &descriptorSets = vertex.descriptorSets;

        auto &graphicsPipeline = pipeline.graphicsPipeline;
        auto &pipelineLayout = pipeline.pipelineLayout;

        commandBuffers[currentFrame].begin({});

        // 转换交换链图像布局
        transition_image_layout(swapChainImages[imageIndex], vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eColorAttachmentOptimal, {},
                                vk::AccessFlagBits2::eColorAttachmentWrite,
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                vk::ImageAspectFlagBits::eColor);

        // 转换深度图像布局
        transition_image_layout(*depthImage, vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eDepthAttachmentOptimal,
                                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                                vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                    vk::PipelineStageFlagBits2::eLateFragmentTests,
                                vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                    vk::PipelineStageFlagBits2::eLateFragmentTests,
                                vk::ImageAspectFlagBits::eDepth);

        vk::ClearValue clearColor = vk::ClearColorValue(0.1f, 0.1f, 0.1f, 1.0f);
        vk::ClearValue clearDepth =
            vk::ClearDepthStencilValue{.depth = 1.0f, .stencil = 0};

        vk::RenderingAttachmentInfo colorAttachmentInfo = {
            .imageView = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColor};

        vk::RenderingAttachmentInfo depthAttachmentInfo = {
            .imageView = depthImageView,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = clearDepth};

        vk::RenderingInfo renderingInfo = {
            .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo};

        commandBuffers[currentFrame].beginRendering(renderingInfo);

        // 设置动态拓扑状态
        commandBuffers[currentFrame].setPrimitiveTopology(vertex.currentTopology);
        commandBuffers[currentFrame].setPrimitiveRestartEnable(
            vertex.primitiveRestartEnable);

        commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                  *graphicsPipeline);
        commandBuffers[currentFrame].setViewport(
            0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                            static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffers[currentFrame].setScissor(
            0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, {0});
        commandBuffers[currentFrame].bindIndexBuffer(*indexBuffer, 0,
                                                     vk::IndexType::eUint16);
        commandBuffers[currentFrame].bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
            *descriptorSets[currentFrame], nullptr);

        // 绘制命令
        commandBuffers[currentFrame].drawIndexed(vertex.currentIndices->size(), 1, 0, 0,
                                                 0);

        commandBuffers[currentFrame].endRendering();

        // 转换回呈现布局
        transition_image_layout(
            swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite,
            {}, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe, vk::ImageAspectFlagBits::eColor);
        commandBuffers[currentFrame].end();
    }

  public:
    void setup_drawing(::mcs::vulkan::vulkan_device &vulkan_device,
                       swapchain_type &swapchain)
    {
        createCommandBuffers(vulkan_device);
        createSyncObjects(vulkan_device, swapchain);
    }

    void drawFrame(::mcs::vulkan::vulkan_device &vulkan_device, swapchain_type &swapchain,
                   window_type &window, vulkan_vertex &vertex,
                   graphics_pipeline &pipeline)
    {
        auto &device = vulkan_device.device;
        vk::raii::Queue &queue = vulkan_device.queue;

        auto &swapChain = swapchain.swapChain;
        auto &swapChainExtent = swapchain.swapChainExtent;
        auto &framebufferResized = window.ref_framebuffer_size();
        auto &uniformBuffersMapped = vertex.uniformBuffersMapped;

        // 检查并切换拓扑
        frameCounter++;
        if (frameCounter >= framesPerTopology)
        {
            frameCounter = 0;
            currentTopologyIndex = (currentTopologyIndex + 1) % topologies.size();
            vertex.setTopology(topologies[currentTopologyIndex], vulkan_device);
            std::cout << "Switched to: " << topologyNames[currentTopologyIndex]
                      << std::endl;
        }

        while (vk::Result::eTimeout ==
               device.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX))
            ;

        auto [result, imageIndex] = swapChain.acquireNextImage(
            UINT64_MAX, *presentCompleteSemaphore[semaphoreIndex], nullptr);

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            swapchain.recreateSwapChain(vulkan_device, window);
            return;
        }
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(currentFrame, swapChainExtent, uniformBuffersMapped);
        device.resetFences(*inFlightFences[currentFrame]);
        commandBuffers[currentFrame].reset();
        recordCommandBuffer(imageIndex, swapchain, vertex, pipeline);

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
                swapchain.recreateSwapChain(vulkan_device, window);
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
                swapchain.recreateSwapChain(vulkan_device, window);
                return;
            }
            throw;
        }
        semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void cleanup()
    {
        inFlightFences.clear();
        renderFinishedSemaphore.clear();
        presentCompleteSemaphore.clear();
        commandBuffers.clear();
    }
};

struct application_context
{
    using swapchain_type = glfw_namespace::swapchain;
    using window_type = glfw_namespace::window;

    window_type window;
    mcs::vulkan::vulkan_instace instance;
    swapchain_type swapchain;
    mcs::vulkan::vulkan_device device;
    drawing draw;
    vulkan_texture texture;
    graphics_pipeline pipeline;
    vulkan_vertex vertex;

    void set_up()
    {
        // 1. window init
        window_type::init_extensions_to_vulkan();
        // 2. createInstance
        instance.setup_instance();
        // 3. setupDebugMessenger
        instance.setup_dabug();
        // 4. createSurface
        window.setup_surface(instance.instance);
        // 5. pickPhysicalDevice,createLogicalDevice
        device.setup_device(instance.instance, window.surface());
        // 6. createSwapChain,createImageViews
        swapchain.setup_swapchain(device, window);
        // 7. createDescriptorSetLayout,createGraphicsPipeline
        pipeline.setup_pipeline(device, swapchain);
        // 8. createCommandPool
        device.createCommandPool();
        // 9. createDepthResources
        swapchain.createDepthResources(device);
        // 10. createTextureImage,createTextureImageView,createTextureSampler
        texture.setup_texture(device, swapchain);
        // 11.
        // createVertexBuffer,createIndexBuffer,createUniformBuffers,createDescriptorPool,createDescriptorSets
        vertex.setup_vertex(pipeline, device, texture);
        // 12. createCommandBuffers,createSyncObjects
        draw.setup_drawing(device, swapchain);

        // 设置初始拓扑为点列表
        vertex.setTopology(vk::PrimitiveTopology::ePointList, device);
    };

    void cleanup()
    {
        device.device.waitIdle();
        draw.cleanup();
        vertex.cleanup();
        texture.cleanup();
        swapchain.cleanup();
        pipeline.cleanup();
        device.cleanup();
        std::destroy_at(&window.surface());
        window.cleanup();
        instance.cleanup();
    }

    void run()
    {
        window.setup_window();
        set_up();
        mainLoop();
        cleanup();
    }

    void mainLoop()
    {
        while (!window.is_close())
        {
            window.poll_events();
            draw.drawFrame(device, swapchain, window, vertex, pipeline);
        }
        device.device.waitIdle();
    }
};

struct my_applation
{
    application_context &ctx;
    void run()
    {
        ctx.run();
    }
};

int main()
{
    try
    {
        application_context ctx;
        my_applation app{ctx};
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}