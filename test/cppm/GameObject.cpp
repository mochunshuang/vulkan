#include <memory>
#include <utility>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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
constexpr int UI_COUNT = 3;

// 输入状态结构体
struct InputState
{
    bool keys[GLFW_KEY_LAST] = {false};
    bool mouseButtons[GLFW_MOUSE_BUTTON_LAST] = {false};
    double mouseX = 0.0;
    double mouseY = 0.0;
    double mouseDeltaX = 0.0;
    double mouseDeltaY = 0.0;
    double scrollDelta = 0.0;

    void updateMousePosition(double x, double y)
    {
        mouseDeltaX = x - mouseX;
        mouseDeltaY = y - mouseY;
        mouseX = x;
        mouseY = y;
    }

    void resetDeltas()
    {
        mouseDeltaX = 0.0;
        mouseDeltaY = 0.0;
        scrollDelta = 0.0;
    }
};

// 变换组件
struct Transform
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 getModelMatrix() const
    {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMat = glm::mat4_cast(rotation);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
        return translation * rotationMat * scaleMat;
    }

    void rotate(float angle, const glm::vec3 &axis)
    {
        rotation = glm::rotate(rotation, angle, axis);
    }

    void translate(const glm::vec3 &translation)
    {
        position += translation;
    }

    void scaleBy(const glm::vec3 &scaling)
    {
        scale *= scaling;
    }
};

// 相机类
class Camera
{
  public:
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 worldUp = glm::vec3(0.0f, -1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 45.0f;

    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.2f;
    float zoomSpeed = 2.0f;

    void updateVectors()
    {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);

        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }

    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true)
    {
        xOffset *= mouseSensitivity;
        yOffset *= mouseSensitivity;

        yaw += xOffset;
        pitch += yOffset;

        if (constrainPitch)
        {
            pitch = glm::clamp(pitch, -89.0f, 89.0f);
        }

        updateVectors();
    }

    void processMouseScroll(float yOffset)
    {
        fov -= yOffset * zoomSpeed;
        fov = glm::clamp(fov, 1.0f, 45.0f);
    }

    void processKeyboard(int direction, float deltaTime)
    {
        float velocity = movementSpeed * deltaTime;

        switch (direction)
        {
        case GLFW_KEY_W:
            position += front * velocity;
            break;
        case GLFW_KEY_S:
            position -= front * velocity;
            break;
        case GLFW_KEY_A:
            position -= right * velocity;
            break;
        case GLFW_KEY_D:
            position += right * velocity;
            break;
        case GLFW_KEY_Q:
            position += up * velocity;
            break;
        case GLFW_KEY_E:
            position -= up * velocity;
            break; // NOTE: 方向相反 就改正负号
        }
    }

    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) const
    {
        return glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 100.0f);
    }
};

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

struct UIVertexData
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

const std::array<UIVertexData, UI_COUNT> ui_vertex_data = {
    UIVertexData{.vertices = {{{-0.2f, -0.2f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                              {{0.2f, -0.2f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                              {{0.2f, 0.2f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                              {{-0.2f, 0.2f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}},
                 .indices = {0, 1, 2, 2, 3, 0}},
    UIVertexData{.vertices = {{{-0.8f, 0.6f, 0.0f}, {1.0f, 0.5f, 0.0f}, {1.0f, 0.0f}},
                              {{-0.4f, 0.6f, 0.0f}, {0.5f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                              {{-0.6f, 0.9f, 0.0f}, {0.0f, 0.5f, 1.0f}, {0.5f, 1.0f}}},
                 .indices = {0, 1, 2}},
    UIVertexData{.vertices = {{{0.4f, 0.6f, 0.0f}, {0.8f, 0.2f, 0.8f}, {1.0f, 0.0f}},
                              {{0.8f, 0.6f, 0.0f}, {0.2f, 0.8f, 0.8f}, {0.0f, 0.0f}},
                              {{0.8f, 0.9f, 0.0f}, {0.8f, 0.8f, 0.2f}, {0.0f, 1.0f}},
                              {{0.4f, 0.9f, 0.0f}, {0.3f, 0.3f, 0.9f}, {1.0f, 1.0f}}},
                 .indices = {0, 1, 2, 2, 3, 0}}};

namespace glfw_namespace
{

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
            window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan UI Transform Test", nullptr,
                                       nullptr);
            glfwSetWindowUserPointer(window_, this);
            glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);

            // 设置输入回调
            glfwSetKeyCallback(window_, keyCallback);
            glfwSetMouseButtonCallback(window_, mouseButtonCallback);
            glfwSetCursorPosCallback(window_, cursorPosCallback);
            glfwSetScrollCallback(window_, scrollCallback);
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

        // 输入状态访问
        InputState &getInputState()
        {
            return inputState;
        }

      private:
        GLFWwindow *window_{};
        vk::raii::SurfaceKHR surface_ = nullptr;
        bool framebufferResized_{};
        InputState inputState;

        static void framebufferResizeCallback(GLFWwindow *ptr, int /*width*/,
                                              int /*height*/)
        {
            auto *self = static_cast<window *>(glfwGetWindowUserPointer(ptr));
            self->framebufferResized_ = true;
        }

        static void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                                int mods)
        {
            auto *self = static_cast<struct window *>(glfwGetWindowUserPointer(window));
            if (key >= 0 && key < GLFW_KEY_LAST)
            {
                self->inputState.keys[key] = (action != GLFW_RELEASE);
            }
        }

        static void mouseButtonCallback(GLFWwindow *window, int button, int action,
                                        int mods)
        {
            auto *self = static_cast<struct window *>(glfwGetWindowUserPointer(window));
            if (button >= 0 && button < GLFW_MOUSE_BUTTON_LAST)
            {
                self->inputState.mouseButtons[button] = (action != GLFW_RELEASE);
            }
        }

        static void cursorPosCallback(GLFWwindow *window, double xpos, double ypos)
        {
            auto *self = static_cast<struct window *>(glfwGetWindowUserPointer(window));
            self->inputState.updateMousePosition(xpos, ypos);
        }

        static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
        {
            auto *self = static_cast<struct window *>(glfwGetWindowUserPointer(window));
            self->inputState.scrollDelta = yoffset;
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
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = vk::False};
        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                          .scissorCount = 1};
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False};
        rasterizer.lineWidth = 1.0f;
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

        std::vector dynamicStates = {vk::DynamicState::eViewport,
                                     vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1,
                                                        .pSetLayouts =
                                                            &*descriptorSetLayout,
                                                        .pushConstantRangeCount = 0};

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
        {
            throw std::runtime_error("failed to load texture image!");
        }

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

struct UIObject
{
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;
    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::DeviceMemory indexBufferMemory = nullptr;

    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    std::vector<vk::raii::DescriptorSet> descriptorSets;

    const UIVertexData *vertexData = nullptr;
    uint32_t indexCount = 0;

    // 新增：变换组件
    Transform transform;
    std::string name;

    UIObject(const std::string &objName = "UIObject") : name(objName) {}

    // 新增：点击检测方法
    bool isPointInBounds(const glm::vec3 &worldPoint, float threshold = 0.01f) const
    {
        // 将世界坐标转换到UI对象的局部坐标
        glm::mat4 invModel = glm::inverse(transform.getModelMatrix());
        glm::vec4 localPoint = invModel * glm::vec4(worldPoint, 1.0f);

        // 简单的2D边界检查（假设UI对象主要在XY平面）
        if (vertexData && !vertexData->vertices.empty())
        {
            // 计算边界框
            float minX = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max();
            float maxZ = std::numeric_limits<float>::lowest();

            for (const auto &vertex : vertexData->vertices)
            {
                minX = std::min(minX, vertex.pos.x);
                maxX = std::max(maxX, vertex.pos.x);
                minY = std::min(minY, vertex.pos.y);
                maxY = std::max(maxY, vertex.pos.y);
                minZ = std::min(minZ, vertex.pos.z);
                maxZ = std::max(maxZ, vertex.pos.z);
            }

            // 扩展边界以容错
            minX -= threshold;
            maxX += threshold;
            minY -= threshold;
            maxY += threshold;
            minZ -= threshold;
            maxZ += threshold;

            // 检查点是否在边界内
            return localPoint.x >= minX && localPoint.x <= maxX && localPoint.y >= minY &&
                   localPoint.y <= maxY && localPoint.z >= minZ && localPoint.z <= maxZ;
        }

        return false;
    }

    void createVertexBuffer(::mcs::vulkan::vulkan_device &vulkan_device,
                            const UIVertexData &data)
    {
        vertexData = &data;
        indexCount = static_cast<uint32_t>(data.indices.size());

        vk::DeviceSize bufferSize = sizeof(data.vertices[0]) * data.vertices.size();
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vulkan_device.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                   stagingBuffer, stagingBufferMemory);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, data.vertices.data(), bufferSize);
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
        vk::DeviceSize bufferSize =
            sizeof(vertexData->indices[0]) * vertexData->indices.size();

        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vulkan_device.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                   stagingBuffer, stagingBufferMemory);

        void *data = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, vertexData->indices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        vulkan_device.createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);

        vulkan_device.copyBuffer(stagingBuffer, indexBuffer, bufferSize);
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

    void createDescriptorSets(vk::raii::DescriptorPool &descriptorPool,
                              graphics_pipeline &pipeline,
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

    // 更新变换方法
    void applyTranslation(const glm::vec3 &translation)
    {
        transform.translate(translation);
    }

    void applyRotation(float angle, const glm::vec3 &axis)
    {
        transform.rotate(angle, axis);
    }

    void applyScaling(const glm::vec3 &scaling)
    {
        transform.scaleBy(scaling);
    }

    void resetTransform()
    {
        transform = Transform();
    }
    void printTransformInfo() const
    {
        std::cout << name << " Transform Info:\n";
        std::cout << "  Position: (" << transform.position.x << ", "
                  << transform.position.y << ", " << transform.position.z << ")\n";
        std::cout << "  Scale: (" << transform.scale.x << ", " << transform.scale.y
                  << ", " << transform.scale.z << ")\n";
        auto euler = glm::eulerAngles(transform.rotation);
        std::cout << "  Rotation: (" << glm::degrees(euler.x) << "°, "
                  << glm::degrees(euler.y) << "°, " << glm::degrees(euler.z) << "°)\n";
    }

    void cleanup()
    {
        descriptorSets.clear();
        uniformBuffersMapped.clear();
        uniformBuffersMemory.clear();
        uniformBuffers.clear();
        indexBuffer.clear();
        indexBufferMemory.clear();
        vertexBuffer.clear();
        vertexBufferMemory.clear();
    }
};

struct vulkan_vertex
{
    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::array<UIObject, UI_COUNT> uiObjects;

    vulkan_vertex()
    {
        // 初始化UI对象名称
        uiObjects[0] = UIObject("Center Square");
        uiObjects[1] = UIObject("Left Triangle");
        uiObjects[2] = UIObject("Right Rectangle");
    }

    void createDescriptorPool(vk::raii::Device &device)
    {
        std::array poolSize{
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                                   MAX_FRAMES_IN_FLIGHT * UI_COUNT),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                                   MAX_FRAMES_IN_FLIGHT * UI_COUNT)};
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT * UI_COUNT,
            .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
            .pPoolSizes = poolSize.data()};
        descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
    }

    void setup_vertex(graphics_pipeline &pipeline,
                      ::mcs::vulkan::vulkan_device &vulkan_device,
                      vulkan_texture &texture)
    {
        createDescriptorPool(vulkan_device.device);

        for (int i = 0; i < UI_COUNT; i++)
        {
            uiObjects[i].createVertexBuffer(vulkan_device, ui_vertex_data[i]);
            uiObjects[i].createIndexBuffer(vulkan_device);
            uiObjects[i].createUniformBuffers(vulkan_device);
            uiObjects[i].createDescriptorSets(descriptorPool, pipeline, vulkan_device,
                                              texture);
        }
    }

    void cleanup()
    {
        for (auto &uiObject : uiObjects)
        {
            uiObject.cleanup();
        }
        descriptorPool.clear();
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

    // 新增：相机和时间跟踪
    Camera camera;
    float deltaTime = 0.0f;
    float lastFrameTime = 0.0f;
    int selectedUI = 0; // 当前选中的UI对象

  private:
    // 新增：射线检测方法
    glm::vec3 screenToWorldRay(double mouseX, double mouseY, const vk::Extent2D &extent,
                               const Camera &camera, float aspectRatio)
    {
        // 将屏幕坐标转换为标准化设备坐标
        float x =
            (2.0f * static_cast<float>(mouseX)) / static_cast<float>(extent.width) - 1.0f;
        float y = 1.0f -
                  (2.0f * static_cast<float>(mouseY)) / static_cast<float>(extent.height);

        // 创建在近平面和远平面的点
        glm::vec4 rayStartNDC(x, y, -1.0f, 1.0f);
        glm::vec4 rayEndNDC(x, y, 1.0f, 1.0f);

        // 获取逆变换矩阵
        glm::mat4 invView = glm::inverse(camera.getViewMatrix());
        glm::mat4 invProj = glm::inverse(camera.getProjectionMatrix(aspectRatio));

        // 转换到世界坐标
        glm::vec4 rayStartWorld = invView * invProj * rayStartNDC;
        rayStartWorld /= rayStartWorld.w;

        glm::vec4 rayEndWorld = invView * invProj * rayEndNDC;
        rayEndWorld /= rayEndWorld.w;

        // 返回射线方向
        return glm::normalize(glm::vec3(rayEndWorld) - glm::vec3(rayStartWorld));
    }

    // 新增：简单的射线与平面相交检测
    bool rayPlaneIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir,
                              const glm::vec3 &planeNormal, float planeD,
                              glm::vec3 &intersection)
    {
        float denom = glm::dot(planeNormal, rayDir);
        if (std::abs(denom) > 1e-6)
        {
            float t = -(glm::dot(rayOrigin, planeNormal) + planeD) / denom;
            if (t >= 0)
            {
                intersection = rayOrigin + rayDir * t;
                return true;
            }
        }
        return false;
    }

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

    // 处理输入
    // 修改processInput方法，添加鼠标点击选择
    void processInput(InputState &inputState, vulkan_vertex &vertex,
                      const vk::Extent2D &extent)
    {
        // 相机移动
        if (inputState.keys[GLFW_KEY_W])
            camera.processKeyboard(GLFW_KEY_W, deltaTime);
        if (inputState.keys[GLFW_KEY_S])
            camera.processKeyboard(GLFW_KEY_S, deltaTime);
        if (inputState.keys[GLFW_KEY_A])
            camera.processKeyboard(GLFW_KEY_A, deltaTime);
        if (inputState.keys[GLFW_KEY_D])
            camera.processKeyboard(GLFW_KEY_D, deltaTime);
        if (inputState.keys[GLFW_KEY_Q])
            camera.processKeyboard(GLFW_KEY_Q, deltaTime);
        if (inputState.keys[GLFW_KEY_E])
            camera.processKeyboard(GLFW_KEY_E, deltaTime);

        // 鼠标视角控制 - 增加旋转效果
        if (inputState.mouseButtons[GLFW_MOUSE_BUTTON_RIGHT])
        {
            // 增加旋转速度，使效果更明显
            float rotationScale = 2.0f; // 增加旋转幅度
            camera.processMouseMovement(
                static_cast<float>(inputState.mouseDeltaX) * rotationScale,
                static_cast<float>(inputState.mouseDeltaY) * rotationScale);
        }

        // 鼠标滚轮缩放
        if (inputState.scrollDelta != 0.0)
        {
            camera.processMouseScroll(static_cast<float>(inputState.scrollDelta));
        }

        // UI对象选择 - 键盘方式保持不变
        if (inputState.keys[GLFW_KEY_1])
            selectedUI = 0;
        if (inputState.keys[GLFW_KEY_2])
            selectedUI = 1;
        if (inputState.keys[GLFW_KEY_3])
            selectedUI = 2;

        // 新增：鼠标左键点击选择
        static bool wasLeftMousePressed = false;
        if (inputState.mouseButtons[GLFW_MOUSE_BUTTON_LEFT] && !wasLeftMousePressed)
        {
            wasLeftMousePressed = true;

            // 执行点击检测
            float aspectRatio =
                static_cast<float>(extent.width) / static_cast<float>(extent.height);
            glm::vec3 rayDir = screenToWorldRay(inputState.mouseX, inputState.mouseY,
                                                extent, camera, aspectRatio);

            // 假设UI对象主要在Z=0平面附近
            glm::vec3 planeNormal(0.0f, 0.0f, 1.0f);
            float planeD = 0.0f; // 平面方程: ax + by + cz + d = 0

            glm::vec3 intersection;
            if (rayPlaneIntersection(camera.position, rayDir, planeNormal, planeD,
                                     intersection))
            {
                // 检查与哪个UI对象相交
                int newSelected = -1;
                float minDistance = std::numeric_limits<float>::max();

                for (int i = 0; i < UI_COUNT; i++)
                {
                    if (vertex.uiObjects[i].isPointInBounds(intersection))
                    {
                        // 计算到相机的距离，选择最近的
                        float distance = glm::distance(
                            camera.position,
                            glm::vec3(vertex.uiObjects[i].transform.getModelMatrix() *
                                      glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));

                        if (distance < minDistance)
                        {
                            minDistance = distance;
                            newSelected = i;
                        }
                    }
                }

                if (newSelected != -1)
                {
                    selectedUI = newSelected;
                    std::cout << "Selected UI: " << vertex.uiObjects[selectedUI].name
                              << std::endl;
                }
            }
        }
        else if (!inputState.mouseButtons[GLFW_MOUSE_BUTTON_LEFT])
        {
            wasLeftMousePressed = false;
        }

        // 选中的UI对象变换
        UIObject &currentUI = vertex.uiObjects[selectedUI];
        float transformSpeed = 2.0f * deltaTime;
        float rotationSpeed = 1.0f * deltaTime;
        float scaleSpeed = 1.0f * deltaTime;

        // NOTE: 方向相反就修正
        if (inputState.keys[GLFW_KEY_UP])
            currentUI.applyTranslation(
                glm::vec3(0.0f, -transformSpeed, 0.0f)); // 改为负值
        if (inputState.keys[GLFW_KEY_DOWN])
            currentUI.applyTranslation(glm::vec3(0.0f, transformSpeed, 0.0f)); // 改为正值
        if (inputState.keys[GLFW_KEY_LEFT])
            currentUI.applyTranslation(glm::vec3(+transformSpeed, 0.0f, 0.0f));
        if (inputState.keys[GLFW_KEY_RIGHT])
            currentUI.applyTranslation(glm::vec3(-transformSpeed, 0.0f, 0.0f));
        if (inputState.keys[GLFW_KEY_PAGE_UP])
            currentUI.applyTranslation(glm::vec3(0.0f, 0.0f, transformSpeed));
        if (inputState.keys[GLFW_KEY_PAGE_DOWN])
            currentUI.applyTranslation(glm::vec3(0.0f, 0.0f, -transformSpeed));

        // 旋转
        if (inputState.keys[GLFW_KEY_R])
            currentUI.applyRotation(rotationSpeed, glm::vec3(0.0f, 0.0f, 1.0f));
        if (inputState.keys[GLFW_KEY_F])
            currentUI.applyRotation(rotationSpeed, glm::vec3(1.0f, 0.0f, 0.0f));
        if (inputState.keys[GLFW_KEY_V])
            currentUI.applyRotation(rotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f));

        // 缩放
        if (inputState.keys[GLFW_KEY_EQUAL])
            currentUI.applyScaling(glm::vec3(1.0f + scaleSpeed));
        if (inputState.keys[GLFW_KEY_MINUS])
            currentUI.applyScaling(glm::vec3(1.0f - scaleSpeed));

        // 重置变换
        if (inputState.keys[GLFW_KEY_R])
        {
            if (inputState.keys[GLFW_KEY_LEFT_SHIFT] ||
                inputState.keys[GLFW_KEY_RIGHT_SHIFT])
            {
                currentUI.resetTransform();
            }
        }

        // 打印变换信息
        if (inputState.keys[GLFW_KEY_P])
        {
            currentUI.printTransformInfo();
            // 防止连续打印
            inputState.keys[GLFW_KEY_P] = false;
        }

        // 重置输入delta
        inputState.resetDeltas();
    }

    void updateUniformBuffers(uint32_t currentImage, vk::Extent2D &swapChainExtent,
                              vulkan_vertex &vertex)
    {
        float aspectRatio = static_cast<float>(swapChainExtent.width) /
                            static_cast<float>(swapChainExtent.height);

        for (int uiIndex = 0; uiIndex < UI_COUNT; uiIndex++)
        {
            UniformBufferObject ubo{};
            ubo.model = vertex.uiObjects[uiIndex].transform.getModelMatrix();
            ubo.view = camera.getViewMatrix();
            ubo.proj = camera.getProjectionMatrix(aspectRatio);
            ubo.proj[1][1] *= -1; // Vulkan的Y轴翻转

            memcpy(vertex.uiObjects[uiIndex].uniformBuffersMapped[currentImage], &ubo,
                   sizeof(ubo));
        }
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

        auto &pipelineLayout = pipeline.pipelineLayout;

        commandBuffers[currentFrame].begin({});

        // 转换交换链图像布局
        transition_image_layout(swapChainImages[imageIndex], vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                {}, // srcAccessMask
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

        // 为每个UI对象绘制
        for (int uiIndex = 0; uiIndex < UI_COUNT; uiIndex++)
        {
            auto &uiObject = vertex.uiObjects[uiIndex];

            commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                      *pipeline.graphicsPipeline);
            commandBuffers[currentFrame].setViewport(
                0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                                static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
            commandBuffers[currentFrame].setScissor(
                0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
            commandBuffers[currentFrame].bindVertexBuffers(0, *uiObject.vertexBuffer,
                                                           {0});
            commandBuffers[currentFrame].bindIndexBuffer(*uiObject.indexBuffer, 0,
                                                         vk::IndexType::eUint16);
            commandBuffers[currentFrame].bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
                *uiObject.descriptorSets[currentFrame], nullptr);
            commandBuffers[currentFrame].drawIndexed(uiObject.indexCount, 1, 0, 0, 0);
        }

        commandBuffers[currentFrame].endRendering();

        // 转换交换链图像布局回呈现
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

        // 计算deltaTime
        float currentTime = static_cast<float>(glfwGetTime());
        deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // 处理输入 - 传递extent参数
        processInput(window.getInputState(), vertex, swapChainExtent);

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

        updateUniformBuffers(currentFrame, swapChainExtent, vertex);

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
        window_type::init_extensions_to_vulkan();
        instance.setup_instance();
        instance.setup_dabug();
        window.setup_surface(instance.instance);

        device.setup_device(instance.instance, window.surface());
        swapchain.setup_swapchain(device, window);
        pipeline.setup_pipeline(device, swapchain);
        device.createCommandPool();
        swapchain.createDepthResources(device);
        texture.setup_texture(device, swapchain);
        vertex.setup_vertex(pipeline, device, texture);
        draw.setup_drawing(device, swapchain);

        // 打印使用说明
        std::cout << "=== Vulkan UI Transform Test ===\n";
        std::cout << "Camera Controls:\n";
        std::cout << "  W/S - Move forward/backward\n";
        std::cout << "  A/D - Move left/right\n";
        std::cout << "  Q/E - Move down/up\n";
        std::cout << "  Right Mouse Button + Drag - Rotate camera (look around)\n";
        std::cout << "  Mouse Wheel - Zoom in/out (change field of view)\n";
        std::cout << "UI Selection:\n";
        std::cout << "  1/2/3 - Select UI object (1: Center, 2: Left, 3: Right)\n";
        std::cout << "  Left Mouse Click - Click to select UI object\n";
        std::cout << "UI Transform Controls:\n";
        std::cout << "  Arrow Keys - Translate in X/Y plane (screen space)\n";
        std::cout << "  Page Up/Down - Move closer/further in Z axis\n";
        std::cout << "  R/F/V - Rotate around Z/X/Y axis\n";
        std::cout << "  +/- - Scale up/down\n";
        std::cout << "  Shift + R - Reset transform\n";
        std::cout << "  P - Print current transform info\n";
        std::cout << "===============================\n";
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
            glfwPollEvents();
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
// NOLINTEND