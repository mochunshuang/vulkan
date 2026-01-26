#include "./head.hpp"

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <print>
#include <utility>
#include <chrono>
#include <memory>
#include <vector>
#include <functional>

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
    std::vector<VkFence> inFlightFences;
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
constexpr auto TITLE = "Cube with Wireframe Demo";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr auto VERT_SHADER_PATH = "shaders/test_vertex_input_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_vertex_input_frag.spv";
constexpr auto WIREFRAME_FRAG_SHADER_PATH = "shaders/wireframe_frag.spv";

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

class Vertex
{
  public:
    glm::vec2 pos;
    glm::vec3 color;
    float alpha;

    template <typename T>
    static consteval VkFormat mappFormat()
    {
        if constexpr (std::same_as<glm::vec1, T>)
            return VK_FORMAT_R32_SFLOAT;
        else if constexpr (std::same_as<glm::vec2, T>)
            return VK_FORMAT_R32G32_SFLOAT;
        else if constexpr (std::same_as<glm::vec3, T>)
            return VK_FORMAT_R32G32B32_SFLOAT;
        else if constexpr (std::same_as<glm::vec4, T>)
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        else
            throw;
    }

    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return {VkVertexInputAttributeDescription{.location = 0,
                                                  .binding = 0,
                                                  .format =
                                                      mappFormat<decltype(Vertex::pos)>(),
                                                  .offset = offsetof(Vertex, pos)},
                VkVertexInputAttributeDescription{
                    .location = 1,
                    .binding = 0,
                    .format = mappFormat<decltype(Vertex::color)>(),
                    .offset = offsetof(Vertex, color)},
                VkVertexInputAttributeDescription{.location = 2,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32_SFLOAT,
                                                  .offset = offsetof(Vertex, alpha)}};
    }
};

// 简单的Cube几何体
class CubeGeometry
{
  public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> triangleIndices;
    std::vector<uint32_t> lineIndices;

    // 材质属性
    struct Material
    {
        glm::vec3 fillColor = glm::vec3(0.0f, 1.0f, 0.0f);      // 绿色填充
        glm::vec3 wireframeColor = glm::vec3(1.0f, 1.0f, 1.0f); // 白色线框
        float fillAlpha = 1.0f;
        float wireframeAlpha = 1.0f;
        bool showFill = true;
        bool showWireframe = true;
    } material;

    CubeGeometry()
    {
        createCube();
        applyMaterial();
    }

    void createCube()
    {
        vertices.clear();
        triangleIndices.clear();
        lineIndices.clear();

        // 定义立方体的8个顶点
        std::vector<glm::vec3> positions = {
            {-0.5f, -0.5f, 0.0f}, // 0: 左后下
            {0.5f, -0.5f, 0.0f},  // 1: 右后下
            {0.5f, 0.5f, 0.0f},   // 2: 右前下
            {-0.5f, 0.5f, 0.0f},  // 3: 左前下
            {-0.5f, -0.5f, 1.0f}, // 4: 左后上
            {0.5f, -0.5f, 1.0f},  // 5: 右后上
            {0.5f, 0.5f, 1.0f},   // 6: 右前上
            {-0.5f, 0.5f, 1.0f}   // 7: 左前上
        };

        std::vector<glm::vec3> colors = {
            {1.0f, 0.0f, 0.0f}, // 红
            {0.0f, 1.0f, 0.0f}, // 绿
            {0.0f, 0.0f, 1.0f}, // 蓝
            {1.0f, 1.0f, 0.0f}, // 黄
            {1.0f, 0.0f, 1.0f}, // 洋红
            {0.0f, 1.0f, 1.0f}, // 青
            {0.5f, 0.5f, 0.5f}, // 灰
            {1.0f, 1.0f, 1.0f}  // 白
        };

        // 创建顶点（使用2D位置，忽略Z轴用于简单渲染）
        for (size_t i = 0; i < positions.size(); i++)
        {
            vertices.push_back({{positions[i].x, positions[i].y}, // 只使用x,y
                                colors[i],
                                1.0f});
        }

        // 定义12个三角形的索引（2个三角形组成一个面，立方体有6个面）
        triangleIndices = {// 前面
                           3, 2, 6, 3, 6, 7,
                           // 后面
                           0, 4, 5, 0, 5, 1,
                           // 左面
                           0, 3, 7, 0, 7, 4,
                           // 右面
                           1, 5, 6, 1, 6, 2,
                           // 顶面
                           4, 7, 6, 4, 6, 5,
                           // 底面
                           0, 1, 2, 0, 2, 3};

        // 定义线框索引（12条边）
        lineIndices = {// 前面矩形
                       3, 2, 2, 6, 6, 7, 7, 3,
                       // 后面矩形
                       0, 1, 1, 5, 5, 4, 4, 0,
                       // 连接前后
                       0, 3, 1, 2, 4, 7, 5, 6};
    }

    void applyMaterial()
    {
        if (material.showFill)
        {
            // 应用填充颜色到所有顶点
            for (auto &vertex : vertices)
            {
                vertex.color = material.fillColor;
                vertex.alpha = material.fillAlpha;
            }
        }
        else
        {
            // 填充关闭，设置透明度为0
            for (auto &vertex : vertices)
            {
                vertex.alpha = 0.0f;
            }
        }
    }

    void updateMaterial(bool showFill, bool showWireframe, glm::vec3 fillColor,
                        glm::vec3 wireframeColor)
    {
        material.showFill = showFill;
        material.showWireframe = showWireframe;
        material.fillColor = fillColor;
        material.wireframeColor = wireframeColor;
        applyMaterial();
    }
};

// 渲染资源管理类
class RenderMesh
{
  private:
    physical_device &physicalDevice;
    logical_device &device;
    VkQueue queue;
    VkCommandPool commandPool;

    struct BufferResources
    {
        mcs::vulkan::buffer_base vertexBuffer;
        mcs::vulkan::buffer_base triangleIndexBuffer;
        mcs::vulkan::buffer_base lineIndexBuffer;
        bool needsUpdate = true;
    };
    std::array<BufferResources, MAX_FRAMES_IN_FLIGHT> buffers;

    void createBuffer(BufferResources &br, CubeGeometry &geom)
    {
        // 创建顶点缓冲区
        const VkDeviceSize vertexBufferSize =
            sizeof(geom.vertices[0]) * geom.vertices.size();
        br.vertexBuffer =
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = vertexBufferSize,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::staging_buffer stagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = vertexBufferSize,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        stagingBuffer.mapAndUnmapMemory(geom.vertices.data(), vertexBufferSize);
        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 br.vertexBuffer.buffer(), vertexBufferSize);

        // 创建三角形索引缓冲区
        const VkDeviceSize triangleBufferSize =
            sizeof(geom.triangleIndices[0]) * geom.triangleIndices.size();
        br.triangleIndexBuffer = mcs::vulkan::create_buffer(
            physicalDevice, device,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = triangleBufferSize,
             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::staging_buffer triangleStagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = triangleBufferSize,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        triangleStagingBuffer.mapAndUnmapMemory(geom.triangleIndices.data(),
                                                triangleBufferSize);
        mcs::vulkan::copy_buffer(device, queue, commandPool,
                                 triangleStagingBuffer.buffer(),
                                 br.triangleIndexBuffer.buffer(), triangleBufferSize);

        // 创建线框索引缓冲区
        const VkDeviceSize lineBufferSize =
            sizeof(geom.lineIndices[0]) * geom.lineIndices.size();
        br.lineIndexBuffer = mcs::vulkan::create_buffer(
            physicalDevice, device,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = lineBufferSize,
             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::staging_buffer lineStagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = lineBufferSize,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        lineStagingBuffer.mapAndUnmapMemory(geom.lineIndices.data(), lineBufferSize);
        mcs::vulkan::copy_buffer(device, queue, commandPool, lineStagingBuffer.buffer(),
                                 br.lineIndexBuffer.buffer(), lineBufferSize);

        br.needsUpdate = false;
    }

  public:
    CubeGeometry geometry;

    RenderMesh(physical_device &physicalDevice, logical_device &device, VkQueue queue,
               VkCommandPool commandPool)
        : physicalDevice{physicalDevice}, device{device}, queue{queue},
          commandPool{commandPool}
    {
    }

    void updateBuffers()
    {
        for (auto &buffer : buffers)
        {
            buffer.needsUpdate = true;
        }
    }

    void syncBuffers(uint32_t currentFrame)
    {
        if (buffers[currentFrame].needsUpdate)
        {
            createBuffer(buffers[currentFrame], geometry);
        }
    }

    void bind(VkCommandBuffer commandBuffer, uint32_t currentFrame) const
    {
        VkBuffer vertexBuffer = buffers[currentFrame].vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0};
        ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    }

    void drawTriangles(VkCommandBuffer commandBuffer, uint32_t currentFrame) const
    {
        ::vkCmdBindIndexBuffer(commandBuffer,
                               buffers[currentFrame].triangleIndexBuffer.buffer(), 0,
                               VK_INDEX_TYPE_UINT32);
        ::vkCmdDrawIndexed(commandBuffer,
                           static_cast<uint32_t>(geometry.triangleIndices.size()), 1, 0,
                           0, 0);
    }

    void drawLines(VkCommandBuffer commandBuffer, uint32_t currentFrame) const
    {
        ::vkCmdBindIndexBuffer(commandBuffer,
                               buffers[currentFrame].lineIndexBuffer.buffer(), 0,
                               VK_INDEX_TYPE_UINT32);
        ::vkCmdDrawIndexed(commandBuffer,
                           static_cast<uint32_t>(geometry.lineIndices.size()), 1, 0, 0,
                           0);
    }
};

int main()
{
    try
    {
        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                {.features = {.fillModeNonSolid = VK_TRUE, .wideLines = VK_TRUE}},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

        surface window{};
        window.setup({.width = WIDTH, .height = HEIGHT}, TITLE);

        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "Cube Wireframe Demo",
                                    .applicationVersion = VkApiVersion(1, 0, 0),
                                    .pEngineName = "No Engine",
                                    .engineVersion = VkApiVersion(1, 0, 0),
                                    .apiVersion = VkApiVersion(0, 1, 3, 0)});

        VkSurfaceKHR surface_ = window.createVkSurfaceKHR(instance.ref_data());
        assert(surface_ != nullptr);

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
                        auto &query_features = query.head().features;
                        auto &query_vulkan13_features =
                            query.template get<VkPhysicalDeviceVulkan13Features>();
                        auto &query_extended_dynamic_state_features = query.template get<
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                        return query_features.fillModeNonSolid &&
                               query_features.wideLines &&
                               query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
                               query_extended_dynamic_state_features.extendedDynamicState;
                    })
                .pickPhysicalDevice();

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

        auto *graphicsQueue = logical_device_.getDeviceQueue(graphicsIndex, 0);
        auto *presentQueue = logical_device_.getDeviceQueue(graphicsIndex, 0);

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

        auto createPipeline = [&](VkPolygonMode polygonMode, VkPrimitiveTopology topology,
                                  const char *fragShaderPath = FRAG_SHADER_PATH) {
            mcs::vulkan::shader_module vertshader{logical_device_, VERT_SHADER_PATH};
            mcs::vulkan::shader_module fragshader{logical_device_, fragShaderPath};

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

            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &bindingDescription,
                .vertexAttributeDescriptionCount =
                    static_cast<uint32_t>(attributeDescriptions.size()),
                .pVertexAttributeDescriptions = attributeDescriptions.data()};

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                .sType = sType<VkPipelineInputAssemblyStateCreateInfo>(),
                .topology = topology,
                .primitiveRestartEnable = VK_FALSE};

            VkPipelineViewportStateCreateInfo viewportState = {
                .sType = sType<VkPipelineViewportStateCreateInfo>(),
                .viewportCount = 1,
                .scissorCount = 1};

            VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = polygonMode,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 2.0F};

            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE};

            VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
            VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType = sType<VkPipelineColorBlendStateCreateInfo>(),
                .logicOpEnable = VK_FALSE,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment};

            std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                         VK_DYNAMIC_STATE_SCISSOR,
                                                         VK_DYNAMIC_STATE_LINE_WIDTH};
            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(), .setLayoutCount = 0};

            VkPipelineLayout pipelineLayout = nullptr;
            VkPipeline graphicsPipeline = nullptr;

            try
            {
                pipelineLayout =
                    logical_device_.createPipelineLayout(pipelineLayoutInfo, nullptr);

                structure_chain<VkGraphicsPipelineCreateInfo,
                                VkPipelineRenderingCreateInfo>
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
                         .renderPass = VK_NULL_HANDLE},
                        {.colorAttachmentCount = 1,
                         .pColorAttachmentFormats = &swapChainImageFormat}};

                graphicsPipeline = logical_device_.createGraphicsPipelines(
                    nullptr, 1, pipelineCreateInfoChain.head(), nullptr);
                return std::make_pair(pipelineLayout, graphicsPipeline);
            }
            catch (...)
            {
                if (graphicsPipeline != nullptr)
                    logical_device_.destroyPipeline(graphicsPipeline, nullptr);
                if (pipelineLayout != nullptr)
                    logical_device_.destroyPipelineLayout(pipelineLayout, nullptr);
                throw;
            }
        };

        auto [fillPipelineLayout, fillPipeline] =
            createPipeline(VK_POLYGON_MODE_FILL, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        auto [wireframePipelineLayout, wireframePipeline] =
            createPipeline(VK_POLYGON_MODE_LINE, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                           WIREFRAME_FRAG_SHADER_PATH);

        auto *commandPool = logical_device_.createCommandPool(
            {.sType = sType<VkCommandPoolCreateInfo>(),
             .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
             .queueFamilyIndex = graphicsIndex});

        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
        logical_device_.allocateCommandBuffers(
            commandBuffers[0],
            {.sType = sType<VkCommandBufferAllocateInfo>(),
             .commandPool = commandPool,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())});

        frame_context frameContext{logical_device_, swapChainImages.size()};

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

        // 创建渲染网格
        RenderMesh mesh{physicalDevice, logical_device_, graphicsQueue, commandPool};
        mesh.updateBuffers();

        // 定义几种显示模式
        struct DisplayMode
        {
            std::string name;
            bool showFill;
            bool showWireframe;
            glm::vec3 fillColor;
            glm::vec3 wireframeColor;
        };

        std::vector<DisplayMode> displayModes = {
            {"Fill Only (Green)", true, false, glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(1.0f, 1.0f, 1.0f)},
            {"Wireframe Only (White)", false, true, glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(1.0f, 1.0f, 1.0f)},
            {"Fill + Wireframe", true, true, glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(1.0f, 1.0f, 1.0f)},
            {"Red Wireframe Only", false, true, glm::vec3(0.0f, 0.0f, 0.0f),
             glm::vec3(1.0f, 0.0f, 0.0f)},
            {"Blue Fill Only", true, false, glm::vec3(0.0f, 0.0f, 1.0f),
             glm::vec3(1.0f, 1.0f, 1.0f)},
            {"Green Fill + Red Wireframe", true, true, glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(1.0f, 0.0f, 0.0f)}};

        size_t currentMode = 0;
        auto lastModeChange = std::chrono::steady_clock::now();

        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];

            if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};

            VkRenderingAttachmentInfo colorAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = imageView,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

            // 确保缓冲区同步
            mesh.syncBuffers(currentFrame);
            mesh.bind(commandBuffer, currentFrame);

            // 1. 绘制填充（如果启用）
            if (displayModes[currentMode].showFill)
            {
                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    fillPipeline);
                ::vkCmdSetLineWidth(commandBuffer, 1.0f);
                mesh.drawTriangles(commandBuffer, currentFrame);
            }

            // 2. 绘制线框（如果启用）
            if (displayModes[currentMode].showWireframe)
            {
                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    wireframePipeline);
                ::vkCmdSetLineWidth(commandBuffer, 2.0f);
                mesh.drawLines(commandBuffer, currentFrame);
            }

            ::vkCmdEndRendering(commandBuffer);

            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (::vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        };

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
                throw std::runtime_error("failed to acquire swap chain image!");

            ::vkResetFences(device, 1, &inFlightFences[currentFrame]);

            auto *commandBuffer = commandBuffers[currentFrame];
            ::vkResetCommandBuffer(commandBuffer, 0);

            recordCommandBuffer(commandBuffer, currentFrame, imageIndex);

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
                throw std::runtime_error("failed to submit draw command buffer!");

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

        while (window.shouldClose() == 0)
        {
            surface::pollEvents();

            auto now = std::chrono::steady_clock::now();

            // 每2秒切换一次显示模式
            if (now - lastModeChange > std::chrono::seconds(2))
            {
                lastModeChange = now;

                // 更新几何体的材质
                auto &mode = displayModes[currentMode];
                mesh.geometry.updateMaterial(mode.showFill, mode.showWireframe,
                                             mode.fillColor, mode.wireframeColor);
                mesh.updateBuffers();

                // 打印当前模式
                std::println("Mode: {}", mode.name);
                std::println("  Fill: {} (color: [{:.1f}, {:.1f}, {:.1f}])",
                             mode.showFill ? "ON" : "OFF", mode.fillColor.r,
                             mode.fillColor.g, mode.fillColor.b);
                std::println("  Wireframe: {} (color: [{:.1f}, {:.1f}, {:.1f}])",
                             mode.showWireframe ? "ON" : "OFF", mode.wireframeColor.r,
                             mode.wireframeColor.g, mode.wireframeColor.b);
                std::println("");

                // 切换到下一个模式
                currentMode = (currentMode + 1) % displayModes.size();
            }

            drawFrame();
        }

        ::vkDeviceWaitIdle(logical_device_.raw_data());

        if (wireframePipeline != nullptr)
            logical_device_.destroyPipeline(wireframePipeline, nullptr);
        if (wireframePipelineLayout != nullptr)
            logical_device_.destroyPipelineLayout(wireframePipelineLayout, nullptr);
        if (fillPipeline != nullptr)
            logical_device_.destroyPipeline(fillPipeline, nullptr);
        if (fillPipelineLayout != nullptr)
            logical_device_.destroyPipelineLayout(fillPipelineLayout, nullptr);

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