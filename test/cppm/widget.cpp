
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

// ==================== 基础数据结构 ====================

// 通用顶点结构
struct Vertex
{
    glm::vec3 pos;      // 3D位置
    glm::vec3 color;    // RGB颜色
    glm::vec2 texCoord; // 纹理坐标

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

// 统一缓冲区对象
struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// 推送常量 - 用于动态属性
struct PushConstants
{
    alignas(16) glm::mat4 transform;
    alignas(16) glm::vec4 color;
    alignas(4) float pointSize;
    alignas(4) float lineWidth;
};

// ==================== Widget 基类和具体实现 ====================

// Widget基类 - 所有图元的基类
class Widget
{
  public:
    virtual ~Widget() = default;

    // 获取顶点数据
    virtual const std::vector<Vertex> &getVertices() const = 0;

    // 获取索引数据
    virtual const std::vector<uint16_t> &getIndices() const = 0;

    // 获取图元拓扑
    virtual vk::PrimitiveTopology getTopology() const = 0;

    // 是否启用图元重启
    virtual bool enablePrimitiveRestart() const
    {
        return false;
    }

    // 更新Widget状态（动画、变换等）
    virtual void update(float deltaTime) = 0;

    // 获取推送常量数据
    virtual PushConstants getPushConstants() const = 0;

    // 获取Widget名称（用于调试）
    virtual std::string getName() const = 0;
};

// 点Widget
class PointWidget : public Widget
{
  private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    float rotation = 0.0f;
    glm::vec4 baseColor{1.0f, 0.0f, 0.0f, 1.0f};

  public:
    PointWidget()
    {
        // 创建点数据
        vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                    {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},
                    {{-0.3f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.3f, 0.5f}},
                    {{0.3f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.7f, 0.5f}}};
        indices = {0, 1, 2, 3, 4};
    }

    const std::vector<Vertex> &getVertices() const override
    {
        return vertices;
    }
    const std::vector<uint16_t> &getIndices() const override
    {
        return indices;
    }
    vk::PrimitiveTopology getTopology() const override
    {
        return vk::PrimitiveTopology::ePointList;
    }

    void update(float deltaTime) override
    {
        rotation += deltaTime * 45.0f; // 45度/秒
        if (rotation > 360.0f)
            rotation -= 360.0f;
    }

    PushConstants getPushConstants() const override
    {
        PushConstants pc{};
        pc.transform = glm::rotate(glm::mat4(1.0f), glm::radians(rotation),
                                   glm::vec3(0.0f, 0.0f, 1.0f));
        pc.color = baseColor;
        pc.pointSize = 20.0f + 10.0f * std::sin(rotation * 0.1f); // 动态点大小
        pc.lineWidth = 1.0f;
        return pc;
    }

    std::string getName() const override
    {
        return "PointWidget";
    }
};

// 线Widget
class LineWidget : public Widget
{
  private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    float animationTime = 0.0f;

  public:
    LineWidget()
    {
        vertices = {{{-0.8f, -0.3f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                    {{0.8f, -0.3f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                    {{0.0f, 0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}}};
        indices = {0, 1, 1, 2, 2, 0}; // 线列表
    }

    const std::vector<Vertex> &getVertices() const override
    {
        return vertices;
    }
    const std::vector<uint16_t> &getIndices() const override
    {
        return indices;
    }
    vk::PrimitiveTopology getTopology() const override
    {
        return vk::PrimitiveTopology::eLineList;
    }

    void update(float deltaTime) override
    {
        animationTime += deltaTime;
    }

    PushConstants getPushConstants() const override
    {
        PushConstants pc{};
        float scale = 0.8f + 0.2f * std::sin(animationTime * 2.0f);
        pc.transform = glm::scale(glm::mat4(1.0f), glm::vec3(scale, scale, 1.0f));
        pc.color = glm::vec4(0.5f + 0.5f * std::sin(animationTime),
                             0.5f + 0.5f * std::sin(animationTime + 2.0f),
                             0.5f + 0.5f * std::sin(animationTime + 4.0f), 1.0f);
        pc.pointSize = 1.0f;
        pc.lineWidth = 1.0;
        return pc;
    }

    std::string getName() const override
    {
        return "LineWidget";
    }
};

// 三角形Widget
class TriangleWidget : public Widget
{
  private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    float rotation = 0.0f;

  public:
    TriangleWidget()
    {
        vertices = {{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
                    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
                    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
        indices = {0, 1, 2};
    }

    const std::vector<Vertex> &getVertices() const override
    {
        return vertices;
    }
    const std::vector<uint16_t> &getIndices() const override
    {
        return indices;
    }
    vk::PrimitiveTopology getTopology() const override
    {
        return vk::PrimitiveTopology::eTriangleList;
    }

    void update(float deltaTime) override
    {
        rotation += deltaTime * 30.0f; // 30度/秒
        if (rotation > 360.0f)
            rotation -= 360.0f;
    }

    PushConstants getPushConstants() const override
    {
        PushConstants pc{};
        pc.transform = glm::rotate(glm::mat4(1.0f), glm::radians(rotation),
                                   glm::vec3(0.0f, 0.0f, 1.0f));
        pc.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f); // 半透明白色
        pc.pointSize = 1.0f;
        pc.lineWidth = 1.0f;
        return pc;
    }

    std::string getName() const override
    {
        return "TriangleWidget";
    }
};

// ==================== Widget管理器 ====================

class WidgetManager
{
  private:
    std::vector<std::unique_ptr<Widget>> widgets;
    size_t currentWidgetIndex = 0;
    int frameCounter = 0;
    const int framesPerWidget = 300; // 每个Widget显示300帧

  public:
    void addWidget(std::unique_ptr<Widget> widget)
    {
        widgets.push_back(std::move(widget));
    }

    Widget *getCurrentWidget()
    {
        if (widgets.empty())
            return nullptr;
        return widgets[currentWidgetIndex].get();
    }

    void update(float deltaTime)
    {
        if (widgets.empty())
            return;

        // 更新当前Widget
        widgets[currentWidgetIndex]->update(deltaTime);

        // 定期切换Widget
        frameCounter++;
        if (frameCounter >= framesPerWidget)
        {
            frameCounter = 0;
            currentWidgetIndex = (currentWidgetIndex + 1) % widgets.size();
            std::cout << "Switched to: " << widgets[currentWidgetIndex]->getName()
                      << std::endl;
        }
    }

    size_t getWidgetCount() const
    {
        return widgets.size();
    }
};

// ==================== 渲染资源管理 ====================

struct RenderResources
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

// ==================== 窗口和交换链 ====================

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
            window_ =
                glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Widget System", nullptr, nullptr);
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

// ==================== 图形管道 ====================

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

        vk::raii::ShaderModule vertShaderModule = vulkan_device.createShaderModule(
            mcs::vulkan::readFile("shaders/widget_vert.spv"));
        vk::raii::ShaderModule fragShaderModule = vulkan_device.createShaderModule(
            mcs::vulkan::readFile("shaders/widget_frag.spv"));

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShaderModule,
            .pName = "main"};
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragShaderModule,
            .pName = "main"};
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

        // 使用动态拓扑
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::ePointList, // 基础拓扑，会被动态覆盖
            .primitiveRestartEnable = vk::False};

        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                          .scissorCount = 1};

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0f}; // 基础线宽，会被动态覆盖

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
        colorBlendAttachment.blendEnable = vk::True;
        colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
        colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
        colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable = vk::False,
                                                            .logicOp = vk::LogicOp::eCopy,
                                                            .attachmentCount = 1,
                                                            .pAttachments =
                                                                &colorBlendAttachment};

        // 动态状态 - 支持动态拓扑和线宽
        std::vector dynamicStates = {
            vk::DynamicState::eViewport, vk::DynamicState::eScissor,
            vk::DynamicState::ePrimitiveTopology, vk::DynamicState::eLineWidth};

        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()};

        // 推送常量范围
        vk::PushConstantRange pushConstantRange{.stageFlags =
                                                    vk::ShaderStageFlagBits::eVertex |
                                                    vk::ShaderStageFlagBits::eFragment,
                                                .offset = 0,
                                                .size = sizeof(PushConstants)};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = 1,
            .pSetLayouts = &*descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange};

        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::Format depthFormat = swapchain_type::findDepthFormat(vulkan_device);

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

// ==================== 纹理系统 ====================

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

// ==================== 渲染系统 ====================

class RenderSystem
{
  private:
    using swapchain_type = glfw_namespace::swapchain;
    using window_type = glfw_namespace::window;

    ::mcs::vulkan::vulkan_device &vulkan_device;
    swapchain_type &swapchain;
    graphics_pipeline &pipeline;
    vulkan_texture &texture;
    RenderResources &resources;
    WidgetManager &widgetManager;

    std::vector<vk::raii::CommandBuffer> commandBuffers;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphore;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphore;
    std::vector<vk::raii::Fence> inFlightFences;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;

    using time_type = decltype(std::chrono::high_resolution_clock::now());
    time_type currentTime = std::chrono::high_resolution_clock::now();
    time_type lastTime = currentTime;

  public:
    RenderSystem(::mcs::vulkan::vulkan_device &device, swapchain_type &sc,
                 graphics_pipeline &pl, vulkan_texture &tex, RenderResources &res,
                 WidgetManager &wm)
        : vulkan_device(device), swapchain(sc), pipeline(pl), texture(tex),
          resources(res), widgetManager(wm)
    {
    }

    void setup()
    {
        createCommandBuffers();
        createSyncObjects();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
    }

    void updateBuffersForCurrentWidget()
    {
        Widget *currentWidget = widgetManager.getCurrentWidget();
        if (!currentWidget)
            return;

        // 等待设备空闲，确保缓冲区不再被使用
        vulkan_device.device.waitIdle();

        // 清理旧缓冲区
        resources.indexBuffer = nullptr;
        resources.indexBufferMemory = nullptr;
        resources.vertexBuffer = nullptr;
        resources.vertexBufferMemory = nullptr;

        // 创建新缓冲区
        createVertexBuffer(currentWidget->getVertices());
        createIndexBuffer(currentWidget->getIndices());
    }

  private:
    void createVertexBuffer(const std::vector<Vertex> &vertices)
    {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vulkan_device.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                   stagingBuffer, stagingBufferMemory);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, vertices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        vulkan_device.createBuffer(bufferSize,
                                   vk::BufferUsageFlagBits::eTransferDst |
                                       vk::BufferUsageFlagBits::eVertexBuffer,
                                   vk::MemoryPropertyFlagBits::eDeviceLocal,
                                   resources.vertexBuffer, resources.vertexBufferMemory);
        vulkan_device.copyBuffer(stagingBuffer, resources.vertexBuffer, bufferSize);
    }

    void createIndexBuffer(const std::vector<uint16_t> &indices)
    {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vulkan_device.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                   stagingBuffer, stagingBufferMemory);

        void *data = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, indices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        vulkan_device.createBuffer(bufferSize,
                                   vk::BufferUsageFlagBits::eTransferDst |
                                       vk::BufferUsageFlagBits::eIndexBuffer,
                                   vk::MemoryPropertyFlagBits::eDeviceLocal,
                                   resources.indexBuffer, resources.indexBufferMemory);
        vulkan_device.copyBuffer(stagingBuffer, resources.indexBuffer, bufferSize);
    }

    void createCommandBuffers()
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

    void createSyncObjects()
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

    void createUniformBuffers()
    {
        resources.uniformBuffers.clear();
        resources.uniformBuffersMemory.clear();
        resources.uniformBuffersMapped.clear();

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
            resources.uniformBuffers.emplace_back(std::move(buffer));
            resources.uniformBuffersMemory.emplace_back(std::move(bufferMem));
            resources.uniformBuffersMapped.emplace_back(
                resources.uniformBuffersMemory[i].mapMemory(0, bufferSize));
        }
    }

    void createDescriptorPool()
    {
        auto &device = vulkan_device.device;
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
        resources.descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
    }

    void createDescriptorSets()
    {
        auto &descriptorSetLayout = pipeline.descriptorSetLayout;
        vk::raii::Device &device = vulkan_device.device;
        auto &[image, textureSampler] = texture;
        auto &textureImageView = image.imageView;

        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                     descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = resources.descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data()};

        resources.descriptorSets.clear();
        resources.descriptorSets = device.allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo{.buffer = resources.uniformBuffers[i],
                                                .offset = 0,
                                                .range = sizeof(UniformBufferObject)};
            vk::DescriptorImageInfo imageInfo{
                .sampler = textureSampler,
                .imageView = textureImageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
            std::array descriptorWrites{
                vk::WriteDescriptorSet{.dstSet = resources.descriptorSets[i],
                                       .dstBinding = 0,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType =
                                           vk::DescriptorType::eUniformBuffer,
                                       .pBufferInfo = &bufferInfo},
                vk::WriteDescriptorSet{.dstSet = resources.descriptorSets[i],
                                       .dstBinding = 1,
                                       .dstArrayElement = 0,
                                       .descriptorCount = 1,
                                       .descriptorType =
                                           vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &imageInfo}};
            device.updateDescriptorSets(descriptorWrites, {});
        }
    }

    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                                glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj =
            glm::perspective(glm::radians(45.0f),
                             static_cast<float>(swapchain.swapChainExtent.width) /
                                 static_cast<float>(swapchain.swapChainExtent.height),
                             0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(resources.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
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

    void recordCommandBuffer(uint32_t imageIndex)
    {
        auto &swapChainImages = swapchain.swapChainImages;
        auto &swapChainImageViews = swapchain.swapChainImageViews;
        auto &swapChainExtent = swapchain.swapChainExtent;
        auto &[depthImage, _, depthImageView] = swapchain.depth_resource;

        auto &graphicsPipeline = pipeline.graphicsPipeline;
        auto &pipelineLayout = pipeline.pipelineLayout;

        Widget *currentWidget = widgetManager.getCurrentWidget();
        if (!currentWidget)
            return;

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

        // 设置动态状态
        commandBuffers[currentFrame].setPrimitiveTopology(currentWidget->getTopology());
        commandBuffers[currentFrame].setLineWidth(
            currentWidget->getPushConstants().lineWidth);

        commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                  *graphicsPipeline);
        commandBuffers[currentFrame].setViewport(
            0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                            static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffers[currentFrame].setScissor(
            0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        commandBuffers[currentFrame].bindVertexBuffers(0, *resources.vertexBuffer, {0});
        commandBuffers[currentFrame].bindIndexBuffer(*resources.indexBuffer, 0,
                                                     vk::IndexType::eUint16);
        commandBuffers[currentFrame].bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
            *resources.descriptorSets[currentFrame], nullptr);

        // 推送常量
        PushConstants pushConstants = currentWidget->getPushConstants();
        commandBuffers[currentFrame].pushConstants<PushConstants>(
            pipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            pushConstants);

        // 绘制命令
        commandBuffers[currentFrame].drawIndexed(currentWidget->getIndices().size(), 1, 0,
                                                 0, 0);

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
    void drawFrame(window_type &window)
    {
        auto &device = vulkan_device.device;
        vk::raii::Queue &queue = vulkan_device.queue;

        auto &swapChain = swapchain.swapChain;
        auto &swapChainExtent = swapchain.swapChainExtent;
        auto &framebufferResized = window.ref_framebuffer_size();

        // 更新Widget
        currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        widgetManager.update(deltaTime);

        // 检查是否需要更新缓冲区
        static size_t lastWidgetIndex = 0;
        Widget *currentWidget = widgetManager.getCurrentWidget();
        if (currentWidget && lastWidgetIndex != widgetManager.getWidgetCount())
        {
            updateBuffersForCurrentWidget();
            lastWidgetIndex = widgetManager.getWidgetCount();
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

        updateUniformBuffer(currentFrame);
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

// ==================== 应用上下文 ====================

struct application_context
{
    using swapchain_type = glfw_namespace::swapchain;
    using window_type = glfw_namespace::window;

    window_type window;
    mcs::vulkan::vulkan_instace instance;
    swapchain_type swapchain;
    mcs::vulkan::vulkan_device device;
    graphics_pipeline pipeline;
    vulkan_texture texture;
    RenderResources resources;
    WidgetManager widgetManager;
    std::unique_ptr<RenderSystem> renderSystem;

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

        // 11. 初始化Widget系统
        setupWidgets();

        // 12. 初始化渲染系统
        renderSystem = std::make_unique<RenderSystem>(device, swapchain, pipeline,
                                                      texture, resources, widgetManager);
        renderSystem->setup();
    }

    void setupWidgets()
    {
        // 添加各种Widget进行测试
        widgetManager.addWidget(std::make_unique<PointWidget>());
        widgetManager.addWidget(std::make_unique<LineWidget>());
        widgetManager.addWidget(std::make_unique<TriangleWidget>());

        std::cout << "Loaded " << widgetManager.getWidgetCount() << " widgets"
                  << std::endl;
    }

    void cleanup()
    {
        device.device.waitIdle();
        if (renderSystem)
        {
            renderSystem->cleanup();
        }
        resources.cleanup();
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
            renderSystem->drawFrame(window);
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

void checkAndCompareFeatures(VkPhysicalDevice physicalDevice)
{
    if (physicalDevice == VK_NULL_HANDLE)
    {
        std::cerr << "错误：物理设备句柄无效。" << std::endl;
        return;
    }

    // 1. 获取设备基础信息以识别类型
    VkPhysicalDeviceProperties deviceProps = {};
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);

    std::cout << "=== 设备信息 ===" << std::endl;
    std::cout << "设备名称: " << deviceProps.deviceName << std::endl;
    std::cout << "设备类型: ";

    bool isIntegratedGPU = false;
    switch (deviceProps.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        std::cout << "集成显卡 (性能有限，支持特性较少)" << std::endl;
        isIntegratedGPU = true;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        std::cout << "独立显卡 (通常支持更多高级特性)" << std::endl;
        isIntegratedGPU = false;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        std::cout << "虚拟GPU" << std::endl;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        std::cout << "CPU软件实现" << std::endl;
        break;
    default:
        std::cout << "其他类型" << std::endl;
    }

    std::cout << "API版本: " << VK_VERSION_MAJOR(deviceProps.apiVersion) << "."
              << VK_VERSION_MINOR(deviceProps.apiVersion) << "."
              << VK_VERSION_PATCH(deviceProps.apiVersion) << std::endl;

    // 2. 检查扩展支持
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,
                                         nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,
                                         availableExtensions.data());

    bool hasExtendedDynamicState3 = false;
    const char *targetExtension = "VK_EXT_extended_dynamic_state3";

    for (const auto &extension : availableExtensions)
    {
        if (strcmp(extension.extensionName, targetExtension) == 0)
        {
            hasExtendedDynamicState3 = true;
            break;
        }
    }

    std::cout << "\n=== 扩展检查 ===" << std::endl;
    std::cout << "VK_EXT_extended_dynamic_state3: "
              << (hasExtendedDynamicState3 ? "✅ 支持" : "❌ 不支持") << std::endl;

    // 3. 查询特性并展示对比
    std::cout << "\n=== 特性对比 (集成显卡 vs 典型独立显卡) ===" << std::endl;

    // 获取当前设备的实际特性
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    // 为扩展特性准备结构体
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extDynamicState3Features = {};
    extDynamicState3Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;

    // 链接到主查询链
    features2.pNext = &extDynamicState3Features;

    // 执行查询
    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    // 创建对比表格
    std::cout << "| 特性名称                          | 当前设备状态 | 典型独立显卡支持 |"
              << std::endl;
    std::cout << "|----------------------------------|--------------|------------------|"
              << std::endl;

    // 检查一些核心特性
    std::cout << "| samplerAnisotropy (各向异性过滤) | "
              << (features2.features.samplerAnisotropy ? "✅ 支持" : "❌ 不支持")
              << " | ✅ 支持          |" << std::endl;

    std::cout << "| geometryShader (几何着色器)      | "
              << (features2.features.geometryShader ? "✅ 支持" : "❌ 不支持")
              << " | ✅ 支持          |" << std::endl;

    std::cout << "| tessellationShader (细分着色器)  | "
              << (features2.features.tessellationShader ? "✅ 支持" : "❌ 不支持")
              << " | ✅ 支持          |" << std::endl;

    // 检查扩展特性
    if (hasExtendedDynamicState3)
    {
        // 需要查询扩展属性来获取 dynamicPrimitiveTopologyUnrestricted
        VkPhysicalDeviceProperties2 props2 = {};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        VkPhysicalDeviceExtendedDynamicState3PropertiesEXT extProps3 = {};
        extProps3.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT;
        props2.pNext = &extProps3;

        vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

        std::cout << "| dynamicPrimitiveTopologyUnrestricted | "
                  << (extProps3.dynamicPrimitiveTopologyUnrestricted ? "✅ 支持"
                                                                     : "❌ 不支持")
                  << " | " << (isIntegratedGPU ? "❌ 通常不支持" : "✅ 通常支持") << " |"
                  << std::endl;
    }
    else
    {
        std::cout << "| dynamicPrimitiveTopologyUnrestricted | ❌ 扩展不可用 | ✅ "
                     "通常支持     |"
                  << std::endl;
    }

    // 4. 输出总结信息
    std::cout << "\n=== 结果分析 ===" << std::endl;
    if (isIntegratedGPU)
    {
        std::cout << "当前设备为集成显卡，符合预期：" << std::endl;
        std::cout << "1. 可能缺少 geometryShader 等高级着色器支持" << std::endl;
        std::cout << "2. dynamicPrimitiveTopologyUnrestricted 通常不支持" << std::endl;
        std::cout << "3. 基础特性如 samplerAnisotropy 通常支持" << std::endl;
    }
    else
    {
        std::cout << "当前设备为独立显卡，支持情况可能更好：" << std::endl;
        std::cout << "1. 大多数高级特性应该支持" << std::endl;
        std::cout << "2. dynamicPrimitiveTopologyUnrestricted 可能支持" << std::endl;
    }
}
void check()
{
    // Vulkan 实例创建（简略版）
    VkInstance instance;
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        std::cerr << "无法创建 Vulkan 实例" << std::endl;
        return;
    }

    // 获取物理设备（此处仅获取第一个设备）
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        std::cerr << "未找到支持 Vulkan 的物理设备" << std::endl;
        vkDestroyInstance(instance, nullptr);
        return;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = devices[0];

    // 调用检查函数
    checkAndCompareFeatures(physicalDevice);

    // 清理
    vkDestroyInstance(instance, nullptr);
}
int main()
{
    /*
validation layer: type { Validation } msg: vkCmdDrawIndexed(): the last primitive topology
VK_PRIMITIVE_TOPOLOGY_LINE_LIST state set by vkCmdSetPrimitiveTopology is not compatible
with the pipeline topology VK_PRIMITIVE_TOPOLOGY_POINT_LIST. The Vulkan spec states: If
the bound graphics pipeline state was created with the VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
dynamic state enabled and the dynamicPrimitiveTopologyUnrestricted is VK_FALSE, then the
primitiveTopology parameter of vkCmdSetPrimitiveTopology must be of the same topology
class as the pipeline VkPipelineInputAssemblyStateCreateInfo::topology state
(https://vulkan.lunarg.com/doc/view/1.4.321.1/windows/antora/spec/latest/chapters/drawing.html#VUID-vkCmdDrawIndexed-dynamicPrimitiveTopologyUnrestricted-07500)
NOTE: 是受限的
*/
    check();

    try
    {
        // NOTE: 有报错 是因为
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

// NOLINTEND