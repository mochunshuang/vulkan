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
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2; // NOLINT

    // NOLINTBEGIN
    const logical_device *device_{};
    std::vector<VkSemaphore> presentCompleteSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::vector<VkFence> inFlightFences;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;
    // NOLINTEND

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
    // NOLINTNEXTLINE
    static void transition_image_layout(VkCommandBuffer commandBuffer, VkImage image,
                                        VkImageAspectFlags aspectMask,
                                        VkImageLayout oldLayout, VkImageLayout newLayout,
                                        VkAccessFlags srcAccessMask,
                                        VkAccessFlags dstAccessMask, // NOLINT
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask // NOLINT
    )
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
        ::vkCmdPipelineBarrier2(commandBuffer, &dependency_info);
    }
};

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "test_my_triangle";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr auto VERT_SHADER_PATH = "shaders/test_vertex_input_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_vertex_input_frag.spv";

// NOLINTBEGIN
// NOTE: 直接抄写 vulkan.hpp的
template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}
// NOLINTEND

// diff: [Vertex input description] 和 管线 和 shader 高度相关. begin---------------
/*
数据源 (CPU/RAM)              → 如何传输                 → Shader输入
---------------------------------------------------------------------
std::vector<Vertex>           → binding=0, stride=20    → location=0 (pos)
                              → binding=0, offset=0     → vec2 inPosition
                              → binding=0, offset=8     → location=1 (color)
                                                        → vec3 inColor
*/
class Vertex
{
  public:
    glm::vec2 pos;
    glm::vec3 color;

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

    // NOTE: 3. bind描述
    static VkVertexInputBindingDescription getBindingDescription()
    {
        // NOTE: 我们所有的每个顶点数据都打包在一个数组中，所以我们只有一个绑定。
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    // NOTE: 4. attribute 描述
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        // NOTE: 我们有两个属性，位置和颜色，所以我们需要两个属性描述结构
        return {
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0, // NOTE: Vertex::getBindingDescription 只有 0
                .format = mappFormat<decltype(Vertex::pos)>(),
                .offset = offsetof(Vertex, pos)},
            VkVertexInputAttributeDescription{
                .location = 1, // NOTE: 对应shader的 layout(location=1) in vec3 inColor;
                .binding = 0,
                .format = mappFormat<decltype(Vertex::color)>(),
                .offset = offsetof(Vertex, color)}};
    }
};

// NOTE: Vertex buffer、Staging buffer、index buffer 封装在 mesh_data 中
struct mesh_data
{
    // NOLINTBEGIN
    physical_device &physicalDevice;
    logical_device &device;
    VkQueue queue;
    VkCommandPool commandPool;

    using index_type = uint32_t;

    std::vector<Vertex> vertices;
    std::vector<index_type> indices;

    mcs::vulkan::buffer_base vertexBuffer;
    mcs::vulkan::buffer_base indexBuffer;
    // NOLINTEND

    explicit mesh_data(physical_device &physicalDevice, logical_device &device,
                       VkQueue queue, VkCommandPool commandPool,
                       std::vector<Vertex> vertices, std::vector<index_type> indices)
        : physicalDevice{physicalDevice}, device{device}, queue{queue},
          commandPool{commandPool}, vertices{std::move(vertices)},
          indices{std::move(indices)}
    {
        setup();
    }

    void update(std::vector<Vertex> vertices, std::vector<index_type> indices)
    {
        this->vertices = std::move(vertices);
        this->indices = std::move(indices);
        setup();
    }

    template <typename T>
    static consteval VkIndexType mapIndexType()
    {
        if constexpr (std::same_as<T, uint32_t>)
            return VK_INDEX_TYPE_UINT32;
        else if constexpr (std::same_as<T, uint16_t>)
            return VK_INDEX_TYPE_UINT16;
        else if constexpr (std::same_as<T, uint8_t>)
            return VK_INDEX_TYPE_UINT8;
        else
            throw;
    }

    // NOLINTBEGIN
    // 添加：待更新标记和双缓冲
    bool needUpdate = false;
    std::vector<Vertex> queuedVertices;
    std::vector<index_type> queuedIndices;
    // NOLINTEND

    // 只排队，不立即执行
    void queueUpdate(std::vector<Vertex> vertices, std::vector<index_type> indices)
    {
        queuedVertices = std::move(vertices);
        queuedIndices = std::move(indices);
        needUpdate = true;
    }

    // 在绝对安全的时间应用更新
    void applyQueuedUpdate()
    {
        if (!needUpdate)
            return;

        // 1. 将排队数据移动到正式数据（CPU操作，安全）
        vertices = std::move(queuedVertices);
        indices = std::move(queuedIndices);
        needUpdate = false;

        // 2. 现在才操作GPU资源
        safeDestroyBuffers();
        setup();
    }

  private:
    // 添加：安全销毁缓冲区的方法
    void safeDestroyBuffers()
    {
        // 等待GPU完成所有相关工作
        vkQueueWaitIdle(queue);
        // 现在安全销毁
        vertexBuffer = {};
        indexBuffer = {};
    }
    void setup()
    {
        createVertexBuffer();
        createIndexBuffer();
    }

    void syncStatus(const mcs::vulkan::buffer_base &buffer, void *data,
                    const VkDeviceSize &BUFFER_SIZE) const
    {

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
        // NOTE: 无法搬迁到外面很离谱。不知道为什么
        syncStatus(indexBuffer, indices.data(), BUFFER_SIZE);
    }

  public:
    // NOTE: 这里在 recordCommandBuffer 中 被调用
    void bind(VkCommandBuffer commandBuffer) const noexcept
    {
        VkBuffer vertex = vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0}; // NOLINT
        ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, offsets);
        ::vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer(), 0,
                               mapIndexType<index_type>());
    }
    void draw(VkCommandBuffer commandBuffer) const noexcept
    {
        ::vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0,
                           0);
    }
};
// diff: end------------------------------------------------------------

// NOTE: 自源test_my_triangle.cpp，而修改。修改版本： c1: 为前缀
int main()
{
    try
    {

        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}; // NOLINTEND

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                {},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

        // NOTE: 0. surfaceimpl -----------------------------------------------
        surface window{};
        window.setup({.width = WIDTH, .height = HEIGHT}, TITLE); // NOLINT

        // c0: 1.createInstance
        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "Hello Triangle",
                                    .applicationVersion = VkApiVersion(1, 0, 0),
                                    .pEngineName = "No Engine",
                                    .engineVersion = VkApiVersion(1, 0, 0),
                                    // apiVersion必须是应用程序设计使用的Vulkan的最高版本
                                    // //NOTE: SDK支持，AMD应该可能不支持。因此改回了 1.3
                                    .apiVersion = VkApiVersion(0, 1, 3, 0)});
        /*
        [ WARN  ] [debug_extension.hpp:31:static VkBool32 __cdecl
        mcs::vulkan::debug_extension::defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *,
        void *)]: 0 Validation Layer: Warning: Loader Message: Layer
        VK_LAYER_AMD_switchable_graphics uses API version 1.3 which is older than the
        application specified API version of 1.4. May cause issues.
        */

        // c0: 2. createSurface
        VkSurfaceKHR surface_ = window.createVkSurfaceKHR(instance.ref_data());
        assert(surface_ != nullptr);

        // c0: 3. pickPhysicalDevice
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
                        return query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
                               query_extended_dynamic_state_features.extendedDynamicState;
                    })
                .pickPhysicalDevice();
        // c0: 4. graphicsIndex
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
        // c0:: 5. createLogicalDevice
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
        // c0: 6. graphicsQueue
        auto *graphicsQueue = logical_device_.getDeviceQueue(graphicsIndex, 0);

        // c0: 7. presentQueue
        auto presentIndex = [&]() {
            std::vector<VkQueueFamilyProperties> queueFamilyProperties =
                physicalDevice.getQueueFamilyProperties();
            // determine a queueFamilyIndex that supports present
            // first check if the graphicsIndex is good enough
            auto presentIndex =
                physicalDevice.getSurfaceSupportKHR(graphicsIndex, surface_)
                    ? graphicsIndex
                    : ~0;
            if (presentIndex == queueFamilyProperties.size())
            {
                // the graphicsIndex doesn't support present -> look for another family
                // index that supports both graphics and present
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
                    // there's nothing like a single family index that supports both
                    // graphics and present -> look for another family index that supports
                    // present
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

        // c0: 8. createSwapChain
        VkFormat swapChainImageFormat{};
        VkExtent2D swapChainExtent;
        VkSwapchainKHR swapChain{};
        std::vector<VkImage> swapChainImages{};
        // NOTE: 必须lambda 内部一起初始化。注意这是没有办法的。要清理和生成都是一起的
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

            // ✅ 关键：创建和一些上下文有关。封装起来也不难
            swapChain = logical_device_.createSwapchainKHR(swapChainCreateInfo);
            swapChainImages = logical_device_.getSwapchainImagesKHR(swapChain);
        };
        createSwapChain(); // NOTE: 初始化

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

        // c0: 9 .createGraphicsPipeline
        auto createGraphicsPipeline = [&]() {
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

            // NOTE: 什么都写  // NOLINTNEXTLINE
            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                              fragShaderStageInfo};

            // diff: [Vertex input description] ---------------------
            //  NOTE: 5. 管道顶点输入。
            //  需要通过引用createGraphicsPipeline中的结构来设置图形管道以接受这种格式的顶点数据
            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &bindingDescription, // 设置管线如何读取
                .vertexAttributeDescriptionCount =
                    static_cast<uint32_t>(attributeDescriptions.size()),
                .pVertexAttributeDescriptions =
                    attributeDescriptions.data() // 设置顶点输入如何映射到shader
            };
            // diff: -----------------------------------------

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
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                // .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            // NOTE: 配置错误，配置不全，崩溃给你看。因此要非常小心
            //  auto msaaSamples = physicalDevice.getMaxUsableSampleCount();
            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // 没有硬件采样配置
                // NOTE: 9. 这里可以改进内部颜色质量
                .sampleShadingEnable = VK_FALSE,
            };

            VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .blendEnable = VK_FALSE, // 关闭混合
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
            VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType = sType<VkPipelineColorBlendStateCreateInfo>(),
                .logicOpEnable = VK_FALSE,
                .logicOp = VkLogicOp::VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment};

            std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                         VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            // NOTE: 没有描述符集
            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(), .setLayoutCount = 0};

            VkPipelineLayout pipelineLayout = nullptr; // NOLINT
            VkPipeline graphicsPipeline = nullptr;     //

            try
            {
                pipelineLayout =
                    logical_device_.createPipelineLayout(pipelineLayoutInfo, nullptr);

                // Use dynamic rendering
                structure_chain<VkGraphicsPipelineCreateInfo,
                                VkPipelineRenderingCreateInfo>
                    pipelineCreateInfoChain = {
                        {.stageCount = 2,
                         .pStages = shaderStages,
                         .pVertexInputState = &vertexInputInfo,
                         .pInputAssemblyState = &inputAssembly,
                         .pViewportState = &viewportState,
                         .pRasterizationState = &rasterizer,
                         .pMultisampleState = &multisampling, // MSAA 是默认的
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
                {
                    logical_device_.destroyPipeline(graphicsPipeline, nullptr);
                    graphicsPipeline = nullptr;
                }
                if (pipelineLayout != nullptr)
                {
                    logical_device_.destroyPipelineLayout(pipelineLayout, nullptr);
                    pipelineLayout = nullptr;
                }
                throw;
            }
        };
        // pipelineLayout, graphicsPipeline
        auto [pipelineLayout, graphicsPipeline] = createGraphicsPipeline();

        // c0: 10. createCommandPool()
        auto *commandPool = logical_device_.createCommandPool(
            {.sType = sType<VkCommandPoolCreateInfo>(),
             .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
             .queueFamilyIndex = graphicsIndex});

        // c0: 11. commandBuffers
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
        logical_device_.allocateCommandBuffers(
            commandBuffers[0],
            {.sType = sType<VkCommandBufferAllocateInfo>(),
             .commandPool = commandPool,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())});

        // c0: 12. frame_context
        frame_context frameContext{logical_device_, swapChainImages.size()};

        // c0: 13. recreateSwapChain
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

        // c0: 14. recordCommandBuffer
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       mesh_data &input_mesh, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];
            if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // Before starting rendering, transition the swapchain image to
            // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                {}, // srcAccessMask (no need to wait for previous operations)
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};

            VkRenderingAttachmentInfo colorAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = imageView, // NOTE: 原生view 没有经过深度，颜色等处理
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                // NOTE: 进行多重采样解析（MSAA resolve 才用到，配置错误就崩溃
                //  .resolveMode = VkResolveModeFlagBits::VK_RESOLVE_MODE_AVERAGE_BIT,
                //  .resolveImageView = swapChainImageViews[imageIndex],
                //  .resolveImageLayout =
                //      VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

            // NOTE: 绑定
            ::vkCmdBeginRendering(commandBuffer, &renderingInfo);

            ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                graphicsPipeline);

            // NOTE: 管线动态信息设置
            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // NOTE: 硬编码改成CPU端配置上传。将具体内容交给 mesh_data 函数指定
            // diff: 顶点数据录入到命令缓冲区 -------------
            input_mesh.bind(commandBuffer);
            input_mesh.draw(commandBuffer);
            // diff: ---------------------------------------

            ::vkCmdEndRendering(commandBuffer);

            // After rendering, transition the swapchain image to PRESENT_SRC
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

        //  c0: 16. drawFrame
        // diff: 准备好顶点和顶点索引--------------- // NOLINTBEGIN
        const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                              {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                              {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                              {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
        const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        mesh_data input_mesh{physicalDevice, logical_device_, graphicsQueue,
                             commandPool,    vertices,        indices};
        // diff: --------------- // NOLINTEND
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
            uint32_t imageIndex; // NOLINT
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
            // TODO(mcs): 或许需要封装一下 commandBuffers，其实已经封装过了
            ::vkResetFences(device, 1, &inFlightFences[currentFrame]);

            auto *commandBuffer = commandBuffers[currentFrame]; // NOLINT

            ::vkResetCommandBuffer(commandBuffer, 0);

            input_mesh.applyQueuedUpdate(); // diff:  vkResetFences 之后更新顶点
            recordCommandBuffer(commandBuffer, input_mesh, imageIndex);

            // NOLINTNEXTLINE
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

            // NOTE: 提交命令到 graphicsQueue
            if (::vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                                inFlightFences[currentFrame]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            // NOTE: 然后设置呈现：
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

            // NOTE: 最好交替更新CPU可以访问的 当前帧
            semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        };

        //  c0: 17. mainloop
        while (window.shouldClose() == 0)
        {
            surface::pollEvents();

            // 检查是否需要更新形状
            auto now = std::chrono::steady_clock::now();
            static auto lastUpdate = std::chrono::steady_clock::now();
            if (now - lastUpdate > std::chrono::seconds(2))
            {
                lastUpdate = now;

                static bool isTriangle = false;
                if (isTriangle)
                {
                    // 只记录要更新的数据，不立即执行
                    // 切换到三角形
                    const std::vector<Vertex> vertices = {
                        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                        {{0.0f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
                    const std::vector<uint32_t> indices = {0, 1, 2};
                    input_mesh.queueUpdate(vertices, indices);
                }
                else
                {
                    // 只记录要更新的数据，不立即执行
                    const std::vector<Vertex> newVertices = vertices;
                    const std::vector<uint32_t> newIndices = indices;
                    input_mesh.queueUpdate(newVertices, newIndices);
                }
                isTriangle = !isTriangle;
            }

            drawFrame();
        }
        //  c0: 18. destroy. 暂时不知道如何封装。或许
        // NOTE: 9. Don't release anything until the GPU is completely idle.
        ::vkDeviceWaitIdle(logical_device_.raw_data());

        // destroy
        if (graphicsPipeline != nullptr)
        {
            logical_device_.destroyPipeline(graphicsPipeline, nullptr);
            graphicsPipeline = nullptr;
        }
        if (pipelineLayout != nullptr)
        {
            logical_device_.destroyPipelineLayout(pipelineLayout, nullptr);
            pipelineLayout = nullptr;
        }

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
