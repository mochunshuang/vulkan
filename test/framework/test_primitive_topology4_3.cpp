#include "./head.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <memory>
#include <print>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

// ===================================================
// 常量和配置
// ===================================================
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "Two Pipelines Geometry Demo with Depth Test";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr auto VERT_SHADER_PATH = "shaders/primitive_topology4_3_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/primitive_topology4_3_frag.spv";

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

// ===================================================
// 推送常量结构（重新排列以匹配GLSL布局规则）
// ===================================================
struct alignas(16) PushConstants
{
    glm::mat4 mvp;               // 偏移0，大小64字节
    glm::vec4 wireColorOverride; // 偏移64，大小16字节
    int renderMode = 0;          // 偏移80，大小4字节
    float lineWidth = 2.0f;      // 偏移84，大小4字节
    float wireAlpha = 1.0f;      // 偏移88，大小4字节
    // 自动填充4字节，总大小96字节（16字节对齐）

    // 构造函数
    PushConstants() = default;
    PushConstants(const glm::mat4 &m, int mode, float lw, float wa, const glm::vec4 &wco)
        : mvp(m), wireColorOverride(wco), renderMode(mode), lineWidth(lw), wireAlpha(wa)
    {
    }
};

// 静态断言，确保大小正确
static_assert(sizeof(PushConstants) == 96,
              "PushConstants must be 96 bytes for GLSL alignment");

// ===================================================
// 帧上下文结构
// ===================================================
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
        for (auto semaphore : presentCompleteSemaphore)
            ::vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
        presentCompleteSemaphore.clear();

        for (auto semaphore : renderFinishedSemaphore)
            ::vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
        renderFinishedSemaphore.clear();

        for (auto fence : inFlightFences)
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

// ===================================================
// 顶点结构
// ===================================================
class Vertex
{
  public:
    glm::vec3 pos;       // 位置
    glm::vec3 fillColor; // 填充颜色
    glm::vec3 wireColor; // 线框颜色
    float alpha;         // 透明度

    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions()
    {
        return {VkVertexInputAttributeDescription{.location = 0,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                  .offset = offsetof(Vertex, pos)},
                VkVertexInputAttributeDescription{.location = 1,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                  .offset = offsetof(Vertex, fillColor)},
                VkVertexInputAttributeDescription{.location = 2,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                  .offset = offsetof(Vertex, wireColor)},
                VkVertexInputAttributeDescription{.location = 3,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32_SFLOAT,
                                                  .offset = offsetof(Vertex, alpha)}};
    }
};

// ===================================================
// 材质类
// ===================================================
struct Material
{
    glm::vec3 fillColor = glm::vec3(1.0f, 0.0f, 0.0f);               // 默认红色填充
    glm::vec3 wireColor = glm::vec3(1.0f, 1.0f, 1.0f);               // 默认白色线框
    glm::vec4 wireColorOverride = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // RGBA覆盖色
    bool showFill = true;
    bool showWire = false;
    float fillAlpha = 1.0f;
    float wireAlpha = 1.0f;
    float lineWidth = 2.0f;
};

// ===================================================
// Geometry基类
// ===================================================
class Geometry
{
  public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> triangleIndices;
    std::vector<uint32_t> lineIndices;
    Material material;

    Geometry() = default;
    virtual ~Geometry() = default;

    void clear()
    {
        vertices.clear();
        triangleIndices.clear();
        lineIndices.clear();
    }

    void applyMaterial()
    {
        for (auto &vertex : vertices)
        {
            vertex.fillColor = material.fillColor;
            vertex.wireColor = material.wireColor;
            vertex.alpha = material.showFill ? material.fillAlpha : 0.0f;
        }
    }

    void setMaterial(const Material &newMaterial)
    {
        material = newMaterial;
        applyMaterial();
    }
};

// ===================================================
// 图像布局转换辅助函数
// ===================================================
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

// ===================================================
// Mesh渲染类
// ===================================================
class Mesh
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

    void updateBuffer(BufferResources &br, Geometry &geom)
    {
        // 更新顶点缓冲区
        if (!geom.vertices.empty())
        {
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
        }

        // 更新三角形索引缓冲区
        if (!geom.triangleIndices.empty())
        {
            const VkDeviceSize indexBufferSize =
                sizeof(geom.triangleIndices[0]) * geom.triangleIndices.size();
            br.triangleIndexBuffer =
                mcs::vulkan::create_buffer(physicalDevice, device,
                                           {.sType = sType<VkBufferCreateInfo>(),
                                            .size = indexBufferSize,
                                            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            mcs::vulkan::staging_buffer indexStagingBuffer = mcs::vulkan::staging_buffer{
                mcs::vulkan::create_buffer(physicalDevice, device,
                                           {.sType = sType<VkBufferCreateInfo>(),
                                            .size = indexBufferSize,
                                            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

            indexStagingBuffer.mapAndUnmapMemory(geom.triangleIndices.data(),
                                                 indexBufferSize);
            mcs::vulkan::copy_buffer(device, queue, commandPool,
                                     indexStagingBuffer.buffer(),
                                     br.triangleIndexBuffer.buffer(), indexBufferSize);
        }

        // 更新线框索引缓冲区
        if (!geom.lineIndices.empty())
        {
            const VkDeviceSize lineBufferSize =
                sizeof(geom.lineIndices[0]) * geom.lineIndices.size();
            br.lineIndexBuffer =
                mcs::vulkan::create_buffer(physicalDevice, device,
                                           {.sType = sType<VkBufferCreateInfo>(),
                                            .size = lineBufferSize,
                                            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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
            mcs::vulkan::copy_buffer(device, queue, commandPool,
                                     lineStagingBuffer.buffer(),
                                     br.lineIndexBuffer.buffer(), lineBufferSize);
        }

        br.needsUpdate = false;
    }

  public:
    std::unique_ptr<Geometry> geometry;

    Mesh(physical_device &physicalDevice, logical_device &device, VkQueue queue,
         VkCommandPool commandPool)
        : physicalDevice{physicalDevice}, device{device}, queue{queue},
          commandPool{commandPool}
    {
    }

    void setGeometry(std::unique_ptr<Geometry> newGeometry)
    {
        geometry = std::move(newGeometry);
        for (auto &buffer : buffers)
        {
            buffer.needsUpdate = true;
        }
    }

    void syncBuffers(uint32_t currentFrame)
    {
        if (geometry && buffers[currentFrame].needsUpdate)
        {
            updateBuffer(buffers[currentFrame], *geometry);
        }
    }

    void bind(VkCommandBuffer commandBuffer, uint32_t currentFrame) const
    {
        if (!geometry)
            return;

        VkBuffer vertexBuffer = buffers[currentFrame].vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0};
        ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    }

    void drawTriangles(VkCommandBuffer commandBuffer, uint32_t currentFrame) const
    {
        if (!geometry || geometry->triangleIndices.empty())
            return;

        ::vkCmdBindIndexBuffer(commandBuffer,
                               buffers[currentFrame].triangleIndexBuffer.buffer(), 0,
                               VK_INDEX_TYPE_UINT32);
        ::vkCmdDrawIndexed(commandBuffer,
                           static_cast<uint32_t>(geometry->triangleIndices.size()), 1, 0,
                           0, 0);
    }

    void drawLines(VkCommandBuffer commandBuffer, uint32_t currentFrame) const
    {
        if (!geometry || geometry->lineIndices.empty())
            return;

        ::vkCmdBindIndexBuffer(commandBuffer,
                               buffers[currentFrame].lineIndexBuffer.buffer(), 0,
                               VK_INDEX_TYPE_UINT32);
        ::vkCmdDrawIndexed(commandBuffer,
                           static_cast<uint32_t>(geometry->lineIndices.size()), 1, 0, 0,
                           0);
    }
};

// ===================================================
// 主函数
// ===================================================
int main()
{
    try
    {
        // 1. 创建窗口
        surface window{};
        window.setup({.width = WIDTH, .height = HEIGHT}, TITLE);

        // 2. 创建Vulkan实例
        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "Depth Test Demo",
                                    .applicationVersion = VkApiVersion(1, 0, 0),
                                    .pEngineName = "No Engine",
                                    .engineVersion = VkApiVersion(1, 0, 0),
                                    .apiVersion = VkApiVersion(0, 1, 3, 0)});

        VkSurfaceKHR surface_ = window.createVkSurfaceKHR(instance.ref_data());
        assert(surface_ != nullptr);

        // 3. 选择物理设备（启用深度测试所需的特性）
        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                {.features = {.fillModeNonSolid = VK_TRUE,
                              .wideLines = VK_TRUE,
                              .depthClamp = VK_TRUE,
                              .depthBiasClamp = VK_TRUE}},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

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
                               query_features.wideLines && query_features.depthClamp &&
                               query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
                               query_extended_dynamic_state_features.extendedDynamicState;
                    })
                .pickPhysicalDevice();

        // 4. 获取队列族索引
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

        // 5. 创建设备
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

        // 6. 创建交换链
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
            if (surfaceCapabilities.maxImageCount > 0 &&
                minImageCount > surfaceCapabilities.maxImageCount)
                minImageCount = surfaceCapabilities.maxImageCount;

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

        // 7. 创建图像视图
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

        // 8. 创建深度图像和视图
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkImage depthImage = nullptr;
        VkDeviceMemory depthImageMemory = nullptr;
        VkImageView depthImageView = nullptr;

        auto createDepthResources = [&]() {
            // 销毁旧的深度资源
            if (depthImageView)
            {
                logical_device_.destroyImageView(depthImageView, nullptr);
            }
            if (depthImage)
            {
                logical_device_.destroyImage(depthImage, nullptr);
            }
            if (depthImageMemory)
            {
                logical_device_.freeMemory(depthImageMemory, nullptr);
            }

            // 创建深度图像
            VkImageCreateInfo imageInfo = {
                .sType = sType<VkImageCreateInfo>(),
                .imageType = VK_IMAGE_TYPE_2D,
                .format = depthFormat,
                .extent = {swapChainExtent.width, swapChainExtent.height, 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

            depthImage = logical_device_.createImage(imageInfo, nullptr);

            // 分配内存
            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(logical_device_.raw_data(), depthImage,
                                         &memRequirements);

            VkMemoryAllocateInfo allocInfo = {
                .sType = sType<VkMemoryAllocateInfo>(),
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = physicalDevice.findMemoryType(
                    memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

            depthImageMemory = logical_device_.allocateMemory(allocInfo, nullptr);
            logical_device_.bindImageMemory(depthImage, depthImageMemory, 0);

            // 创建深度图像视图
            VkImageViewCreateInfo viewInfo = {
                .sType = sType<VkImageViewCreateInfo>(),
                .image = depthImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = depthFormat,
                .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};

            depthImageView = logical_device_.createImageView(viewInfo, nullptr);
        };

        createDepthResources();

        // 9. 创建帧上下文
        frame_context frameContext{logical_device_, swapChainImages.size()};

        // 10. 创建两个管线：填充管线和线框管线（启用深度测试）
        auto createPipeline = [&](VkPolygonMode polygonMode,
                                  VkPrimitiveTopology topology) {
            mcs::vulkan::shader_module vertshader{logical_device_, VERT_SHADER_PATH};
            mcs::vulkan::shader_module fragshader{logical_device_, FRAG_SHADER_PATH};

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
                .topology = topology, // 固定拓扑
                .primitiveRestartEnable = VK_FALSE};

            VkPipelineViewportStateCreateInfo viewportState = {
                .sType = sType<VkPipelineViewportStateCreateInfo>(),
                .viewportCount = 1,
                .scissorCount = 1};

            VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
                .depthClampEnable = VK_TRUE, // 允许深度截取
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = polygonMode,        // 固定多边形模式
                .cullMode = VK_CULL_MODE_BACK_BIT, // 背面剔除
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

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

            // 深度测试配置
            VkPipelineDepthStencilStateCreateInfo depthStencil = {
                .sType = sType<VkPipelineDepthStencilStateCreateInfo>(),
                .depthTestEnable = VK_TRUE,           // 启用深度测试
                .depthWriteEnable = VK_TRUE,          // 启用深度写入
                .depthCompareOp = VK_COMPARE_OP_LESS, // 深度比较函数：小于
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable = VK_FALSE,
                .minDepthBounds = 0.0f,
                .maxDepthBounds = 1.0f};

            // 动态状态：只保留必要的动态状态
            std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                         VK_DYNAMIC_STATE_SCISSOR,
                                                         VK_DYNAMIC_STATE_LINE_WIDTH};

            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            // 推送常量范围（包含MVP矩阵）- 现在大小是96字节
            VkPushConstantRange pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(PushConstants) // 96字节
            };

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(),
                .setLayoutCount = 0,
                .pSetLayouts = nullptr,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushConstantRange};

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
                         .pDepthStencilState = &depthStencil, // 添加深度测试状态
                         .pColorBlendState = &colorBlending,
                         .pDynamicState = &dynamicState,
                         .layout = pipelineLayout,
                         .renderPass = VK_NULL_HANDLE},
                        {.colorAttachmentCount = 1,
                         .pColorAttachmentFormats = &swapChainImageFormat,
                         .depthAttachmentFormat = depthFormat} // 指定深度附件格式
                    };

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

        // 创建填充管线
        auto [fillPipelineLayout, fillPipeline] =
            createPipeline(VK_POLYGON_MODE_FILL, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        // 创建线框管线
        auto [wirePipelineLayout, wirePipeline] =
            createPipeline(VK_POLYGON_MODE_LINE, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

        // 11. 创建命令池
        auto *commandPool = logical_device_.createCommandPool(
            {.sType = sType<VkCommandPoolCreateInfo>(),
             .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
             .queueFamilyIndex = graphicsIndex});

        // 12. 创建命令缓冲区
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
        logical_device_.allocateCommandBuffers(
            commandBuffers[0],
            {.sType = sType<VkCommandBufferAllocateInfo>(),
             .commandPool = commandPool,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())});

        // 13. 创建Mesh - 使用一个简单的3D Box（8个顶点）
        Mesh mesh{physicalDevice, logical_device_, graphicsQueue, commandPool};

        // 创建一个简单的单位立方体
        auto box = std::make_unique<Geometry>();

        // 定义单位立方体的8个顶点
        box->vertices = {
            // 前平面
            {{-0.5f, -0.5f, 0.5f},
             box->material.fillColor,
             box->material.wireColor,
             1.0f},
            {{0.5f, -0.5f, 0.5f}, box->material.fillColor, box->material.wireColor, 1.0f},
            {{0.5f, 0.5f, 0.5f}, box->material.fillColor, box->material.wireColor, 1.0f},
            {{-0.5f, 0.5f, 0.5f}, box->material.fillColor, box->material.wireColor, 1.0f},

            // 后平面
            {{-0.5f, -0.5f, -0.5f},
             box->material.fillColor,
             box->material.wireColor,
             1.0f},
            {{0.5f, -0.5f, -0.5f},
             box->material.fillColor,
             box->material.wireColor,
             1.0f},
            {{0.5f, 0.5f, -0.5f}, box->material.fillColor, box->material.wireColor, 1.0f},
            {{-0.5f, 0.5f, -0.5f},
             box->material.fillColor,
             box->material.wireColor,
             1.0f}};

        // 三角形索引（12个三角形 = 6个面 × 2个三角形）
        box->triangleIndices = {// 前
                                0, 1, 2, 0, 2, 3,
                                // 右
                                1, 5, 6, 1, 6, 2,
                                // 后
                                5, 4, 7, 5, 7, 6,
                                // 左
                                4, 0, 3, 4, 3, 7,
                                // 上
                                3, 2, 6, 3, 6, 7,
                                // 下
                                4, 5, 1, 4, 1, 0};

        // 线框索引（12条边）
        box->lineIndices = {// 前平面
                            0, 1, 1, 2, 2, 3, 3, 0,
                            // 后平面
                            4, 5, 5, 6, 6, 7, 7, 4,
                            // 连接前后
                            0, 4, 1, 5, 2, 6, 3, 7};

        mesh.setGeometry(std::move(box));

        // 14. 定义显示模式
        enum DisplayMode
        {
            FILL_ONLY,
            WIRE_ONLY,
            BOTH
        };

        DisplayMode currentMode = FILL_ONLY;
        auto lastModeChange = std::chrono::steady_clock::now();
        auto startTime = std::chrono::steady_clock::now();

        // 15. 重新创建交换链函数（包含深度资源）
        auto cleanupSwapChain = [&]() {
            for (auto imageView : swapChainImageViews)
            {
                logical_device_.destroyImageView(imageView, nullptr);
            }
            swapChainImageViews.clear();

            if (depthImageView)
            {
                logical_device_.destroyImageView(depthImageView, nullptr);
                depthImageView = nullptr;
            }
            if (depthImage)
            {
                logical_device_.destroyImage(depthImage, nullptr);
                depthImage = nullptr;
            }
            if (depthImageMemory)
            {
                logical_device_.freeMemory(depthImageMemory, nullptr);
                depthImageMemory = nullptr;
            }

            if (swapChain != nullptr)
            {
                logical_device_.destroySwapchainKHR(swapChain);
                swapChain = nullptr;
            }
        };

        auto recreateSwapChain = [&]() {
            window.waitGoodFramebufferSize();
            logical_device_.waitIdle();

            cleanupSwapChain();
            createSwapChain();
            createImageViews();
            createDepthResources(); // 重新创建深度资源
        };

        // 16. 记录命令缓冲区（添加深度附件和MVP矩阵）
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            // 转换交换链图像布局
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                0, // srcAccessMask
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            // 转换深度图像布局
            my_render::transition_image_layout(
                commandBuffer, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                0, // srcAccessMask
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.1F, 0.1F, 0.1F, 1.0F}}};
            VkClearValue clearDepth = {.depthStencil = {.depth = 1.0f}};

            VkRenderingAttachmentInfo colorAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = imageView,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = clearColor};

            VkRenderingAttachmentInfo depthAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = depthImageView,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue = clearDepth};

            VkRenderingInfo renderingInfo = {
                .sType = sType<VkRenderingInfo>(),
                .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment,
                .pDepthAttachment = &depthAttachment}; // 添加深度附件

            vkCmdBeginRendering(commandBuffer, &renderingInfo);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // 同步缓冲区
            mesh.syncBuffers(currentFrame);
            mesh.bind(commandBuffer, currentFrame);

            // 计算MVP矩阵
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float>(currentTime - startTime).count();

            // 模型矩阵：随时间旋转
            glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                                          glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, time * glm::radians(45.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f));

            // 视图矩阵：相机位置
            glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),  // 相机位置
                                         glm::vec3(0.0f, 0.0f, 0.0f),  // 观察目标
                                         glm::vec3(0.0f, 1.0f, 0.0f)); // 上方向

            // 投影矩阵：透视投影
            glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);

            // Vulkan的裁剪空间Y轴是向下的，需要翻转Y轴
            projection[1][1] *= -1;

            glm::mat4 mvp = projection * view * model;

            // 根据模式绘制
            if (currentMode == FILL_ONLY || currentMode == BOTH)
            {
                // 绑定填充管线
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  fillPipeline);

                // 设置线宽（用于线框模式，填充模式忽略）
                vkCmdSetLineWidth(commandBuffer, 1.0f);

                // 设置推送常量：填充模式
                PushConstants pc(mvp, 0, 1.0f, mesh.geometry->material.wireAlpha,
                                 mesh.geometry->material.wireColorOverride);

                vkCmdPushConstants(commandBuffer, fillPipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT |
                                       VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(PushConstants), &pc);

                // 绘制三角形
                mesh.drawTriangles(commandBuffer, currentFrame);
            }

            if (currentMode == WIRE_ONLY || currentMode == BOTH)
            {
                // 绑定线框管线
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  wirePipeline);

                // 设置线宽
                vkCmdSetLineWidth(commandBuffer, mesh.geometry->material.lineWidth);

                // 设置推送常量：线框模式
                PushConstants pc(mvp, 1, mesh.geometry->material.lineWidth,
                                 mesh.geometry->material.wireAlpha,
                                 mesh.geometry->material.wireColorOverride);

                vkCmdPushConstants(commandBuffer, wirePipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT |
                                       VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(PushConstants), &pc);

                // 绘制线框
                mesh.drawLines(commandBuffer, currentFrame);
            }

            vkCmdEndRendering(commandBuffer);

            // 转换回呈现布局
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                0, // dstAccessMask
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        };

        // 17. 主渲染循环
        while (window.shouldClose() == 0)
        {
            window.pollEvents();

            // 每2秒切换一次显示模式
            auto now = std::chrono::steady_clock::now();
            if (now - lastModeChange > std::chrono::seconds(2))
            {
                lastModeChange = now;

                currentMode = static_cast<DisplayMode>((currentMode + 1) % 3);

                // 更新材质
                if (mesh.geometry)
                {
                    Material newMaterial = mesh.geometry->material;

                    switch (currentMode)
                    {
                    case FILL_ONLY:
                        newMaterial.showFill = true;
                        newMaterial.showWire = false;
                        newMaterial.fillColor = glm::vec3(1.0f, 0.0f, 0.0f); // 红色填充
                        newMaterial.wireColorOverride = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
                        newMaterial.lineWidth = 1.0f;
                        newMaterial.wireAlpha = 0.5f;
                        break;
                    case WIRE_ONLY:
                        newMaterial.showFill = false;
                        newMaterial.showWire = true;
                        newMaterial.fillColor = glm::vec3(0.0f, 1.0f, 0.0f); // 绿色填充
                        newMaterial.wireColorOverride =
                            glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // 红色线框
                        newMaterial.lineWidth = 3.0f;
                        newMaterial.wireAlpha = 1.0f;
                        break;
                    case BOTH:
                        newMaterial.showFill = true;
                        newMaterial.showWire = true;
                        newMaterial.fillColor = glm::vec3(0.0f, 0.0f, 1.0f); // 蓝色填充
                        newMaterial.wireColorOverride =
                            glm::vec4(1.0f, 1.0f, 0.0f, 0.8f); // 黄色线框
                        newMaterial.lineWidth = 2.0f;
                        newMaterial.wireAlpha = 0.8f;
                        break;
                    }

                    mesh.geometry->setMaterial(newMaterial);
                }

                // 打印当前模式
                const char *modeNames[] = {"Fill Only (Red)", "Wire Only (Red Wireframe)",
                                           "Both (Blue Fill + Yellow Wireframe)"};
                std::println("Mode: {}", modeNames[currentMode]);
            }

            // 等待上一帧完成
            vkWaitForFences(logical_device_.raw_data(), 1,
                            &frameContext.inFlightFences[frameContext.currentFrame],
                            VK_TRUE, UINT64_MAX);
            vkResetFences(logical_device_.raw_data(), 1,
                          &frameContext.inFlightFences[frameContext.currentFrame]);

            // 获取交换链图像
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(
                logical_device_.raw_data(), swapChain, UINT64_MAX,
                frameContext.presentCompleteSemaphore[frameContext.semaphoreIndex],
                VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreateSwapChain();
                continue;
            }
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            // 记录命令缓冲区
            vkResetCommandBuffer(commandBuffers[frameContext.currentFrame], 0);
            recordCommandBuffer(commandBuffers[frameContext.currentFrame],
                                frameContext.currentFrame, imageIndex);

            // 提交命令缓冲区
            VkPipelineStageFlags waitStages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            VkSubmitInfo submitInfo = {
                .sType = sType<VkSubmitInfo>(),
                .waitSemaphoreCount = 1,
                .pWaitSemaphores =
                    &frameContext.presentCompleteSemaphore[frameContext.semaphoreIndex],
                .pWaitDstStageMask = waitStages,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffers[frameContext.currentFrame],
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &frameContext.renderFinishedSemaphore[imageIndex]};

            if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                              frameContext.inFlightFences[frameContext.currentFrame]) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            // 呈现
            VkPresentInfoKHR presentInfo = {
                .sType = sType<VkPresentInfoKHR>(),
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &frameContext.renderFinishedSemaphore[imageIndex],
                .swapchainCount = 1,
                .pSwapchains = &swapChain,
                .pImageIndices = &imageIndex};

            result = vkQueuePresentKHR(presentQueue, &presentInfo);
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                window.refFramebufferResized())
            {
                window.refFramebufferResized() = false;
                recreateSwapChain();
            }
            else if (result != VK_SUCCESS)
            {
                throw std::runtime_error("failed to present swap chain image!");
            }

            // 更新索引
            frameContext.semaphoreIndex = (frameContext.semaphoreIndex + 1) %
                                          frameContext.presentCompleteSemaphore.size();
            frameContext.currentFrame =
                (frameContext.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        // 18. 清理
        vkDeviceWaitIdle(logical_device_.raw_data());

        // 销毁管线
        vkDestroyPipeline(logical_device_.raw_data(), fillPipeline, nullptr);
        vkDestroyPipelineLayout(logical_device_.raw_data(), fillPipelineLayout, nullptr);
        vkDestroyPipeline(logical_device_.raw_data(), wirePipeline, nullptr);
        vkDestroyPipelineLayout(logical_device_.raw_data(), wirePipelineLayout, nullptr);

        // 销毁深度资源
        cleanupSwapChain();

        // 销毁命令池
        vkDestroyCommandPool(logical_device_.raw_data(), commandPool, nullptr);

        // 销毁表面和设备
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface_);

        std::println("Demo completed successfully!");
    }
    catch (std::exception &e)
    {
        std::println("Error: {}", e.what());
        return -1;
    }

    return 0;
}