#include "./head.hpp"

#include <array>
#include <cassert>
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
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
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
// diff: [new] 修改着色器路径
constexpr auto VERT_SHADER_PATH = "shaders/test_bindless_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_bindless_frag.spv";

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

// diff: [new] 推送常量结构体，用于传递设备地址
struct PushConstants
{
    VkDeviceAddress vertexBufferAddress; // 顶点缓冲区的设备地址
    VkDeviceAddress indexBufferAddress;  // 索引缓冲区的设备地址
};

// diff: [new] 使用bindless的Vertex结构
class Vertex
{
  public:
    glm::vec2 pos;
    glm::vec3 color;

    // 注意：移除了 getBindingDescription 和 getAttributeDescriptions 方法
    // 因为我们使用设备地址直接访问数据，而不是传统的顶点绑定
};

// diff: [new] 修改mesh_data以支持设备地址
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

    // 双缓冲顶点数据：每个飞行帧都有自己的缓冲区
    struct FrameBuffers
    {
        mcs::vulkan::buffer_base vertexBuffer;
        mcs::vulkan::buffer_base indexBuffer;
        VkDeviceAddress vertexBufferAddress = 0; // diff: [new] 设备地址
        VkDeviceAddress indexBufferAddress = 0;  // diff: [new] 设备地址
        bool needsUpdate = true;                 // 标记是否需要更新
    };
    std::array<FrameBuffers, MAX_FRAMES_IN_FLIGHT> frameBuffers;
    // NOLINTEND

    explicit mesh_data(physical_device &physicalDevice, logical_device &device,
                       VkQueue queue, VkCommandPool commandPool,
                       std::vector<Vertex> vertices, std::vector<index_type> indices)
        : physicalDevice{physicalDevice}, device{device}, queue{queue},
          commandPool{commandPool}, vertices{std::move(vertices)},
          indices{std::move(indices)}
    {
        setupAllBuffers();
    }

    // 初始化所有帧的缓冲区
    void setupAllBuffers()
    {
        for (auto &fb : frameBuffers)
        {
            createVertexBufferForFrame(fb);
            createIndexBufferForFrame(fb);
            // diff: [new] 获取设备地址
            getBufferDeviceAddresses(fb);
            fb.needsUpdate = false;
        }
    }

    // diff: [new] 获取缓冲区的设备地址
    void getBufferDeviceAddresses(FrameBuffers &fb)
    {
        if (fb.vertexBuffer.buffer() != VK_NULL_HANDLE)
        {
            VkBufferDeviceAddressInfo vertexAddressInfo = {
                .sType = sType<VkBufferDeviceAddressInfo>(),
                .buffer = fb.vertexBuffer.buffer()};
            fb.vertexBufferAddress =
                ::vkGetBufferDeviceAddress(device.raw_data(), &vertexAddressInfo);
        }

        if (fb.indexBuffer.buffer() != VK_NULL_HANDLE)
        {
            VkBufferDeviceAddressInfo indexAddressInfo = {
                .sType = sType<VkBufferDeviceAddressInfo>(),
                .buffer = fb.indexBuffer.buffer()};
            fb.indexBufferAddress =
                ::vkGetBufferDeviceAddress(device.raw_data(), &indexAddressInfo);
        }
    }

    // NOLINTBEGIN
    // 更新排队数据
    std::vector<Vertex> queuedVertices;
    std::vector<index_type> queuedIndices;
    bool needUpdate = false;
    // NOLINTEND

    // 只排队，不立即执行
    void queueUpdate(std::vector<Vertex> vertices, std::vector<index_type> indices)
    {
        queuedVertices = std::move(vertices);
        queuedIndices = std::move(indices);
        needUpdate = true;

        // 标记所有帧的缓冲区都需要更新
        for (auto &fb : frameBuffers)
        {
            fb.needsUpdate = true;
        }
    }

    // 在录制命令缓冲区时应用更新（针对特定帧）
    void applyQueuedUpdate(uint32_t currentFrame)
    {
        if (!needUpdate)
        {
            return;
        }

        // 只有这个帧的缓冲区需要更新时才更新
        if (auto &fb = frameBuffers[currentFrame]; fb.needsUpdate) // NOLINT
        {
            // 更新CPU端数据
            vertices = queuedVertices;
            indices = queuedIndices;

            // 更新GPU缓冲区（这个帧当前没有被GPU使用）
            createVertexBufferForFrame(fb);
            createIndexBufferForFrame(fb);
            // diff: [new] 更新设备地址
            getBufferDeviceAddresses(fb);
            fb.needsUpdate = false;
        }
        needUpdate = false;
        for (auto &fb : frameBuffers)
            if (fb.needsUpdate)
                needUpdate = true;
    }

  private:
    void syncStatusForFrame(FrameBuffers &fb, void *data,
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

        stagingBuffer.mapAndUnmapMemory(data, static_cast<size_t>(BUFFER_SIZE));

        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 fb.vertexBuffer.buffer(), BUFFER_SIZE);
    }

    // diff: [new] 查找内存类型
    // 可选简化：查找内存类型的测试分配可以简化
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties,
                            VkMemoryAllocateFlags allocateFlags = 0) const
    {
        // 可以简化：对于设备地址内存，通常直接查找支持设备本地和地址标志的内存类型
        // 不需要进行测试分配
        VkPhysicalDeviceMemoryProperties memProperties;
        ::vkGetPhysicalDeviceMemoryProperties(physicalDevice.raw_data(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                // 简化：直接返回第一个匹配的内存类型
                // 在实际使用中，测试分配可能过于保守
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }
    void createVertexBufferForFrame(FrameBuffers &fb)
    {
        if (vertices.empty())
            return;

        const VkDeviceSize BUFFER_SIZE = sizeof(vertices[0]) * vertices.size();

        // diff: [new] 使用自定义的缓冲区创建函数，以设置正确的内存分配标志
        VkBufferCreateInfo bufferInfo = {.sType = sType<VkBufferCreateInfo>(),
                                         .size = BUFFER_SIZE,
                                         .usage =
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

        VkBuffer vertexBuffer;
        if (::vkCreateBuffer(device.raw_data(), &bufferInfo, nullptr, &vertexBuffer) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        VkMemoryRequirements memRequirements;
        ::vkGetBufferMemoryRequirements(device.raw_data(), vertexBuffer,
                                        &memRequirements);

        // diff: [new] 设置内存分配标志，包含VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        // VkMemoryAllocateFlagsInfo allocateFlagsInfo = {
        //     .sType = sType<VkMemoryAllocateFlagsInfo>(),
        //     .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};

        // VkMemoryAllocateInfo allocInfo = {
        //     .sType = sType<VkMemoryAllocateInfo>(),
        //     .pNext = &allocateFlagsInfo,
        //     .allocationSize = memRequirements.size,
        //     .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
        //                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        //                                       VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT)};

        structure_chain<VkMemoryAllocateInfo, VkMemoryAllocateFlagsInfo> allocInfo = {
            {.allocationSize = memRequirements.size,
             .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                               VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT)},
            {.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT}};

        VkDeviceMemory vertexBufferMemory;
        if (::vkAllocateMemory(device.raw_data(), &allocInfo.head(), nullptr,
                               &vertexBufferMemory) != VK_SUCCESS)
        {
            ::vkDestroyBuffer(device.raw_data(), vertexBuffer, nullptr);
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        if (::vkBindBufferMemory(device.raw_data(), vertexBuffer, vertexBufferMemory,
                                 0) != VK_SUCCESS)
        {
            ::vkFreeMemory(device.raw_data(), vertexBufferMemory, nullptr);
            ::vkDestroyBuffer(device.raw_data(), vertexBuffer, nullptr);
            throw std::runtime_error("failed to bind vertex buffer memory!");
        }

        // 创建临时staging buffer来复制顶点数据
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

        stagingBuffer.mapAndUnmapMemory(vertices.data(),
                                        static_cast<size_t>(BUFFER_SIZE));

        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 vertexBuffer, BUFFER_SIZE);

        // diff: [new] 将创建的缓冲区包装到buffer_base中
        fb.vertexBuffer =
            mcs::vulkan::buffer_base(device, vertexBuffer, vertexBufferMemory);
    }

    void createIndexBufferForFrame(FrameBuffers &fb)
    {
        if (indices.empty())
            return;

        const VkDeviceSize BUFFER_SIZE = sizeof(indices[0]) * indices.size();

        // diff: [new] 使用自定义的缓冲区创建函数
        VkBufferCreateInfo bufferInfo = {
            .sType = sType<VkBufferCreateInfo>(),
            .size = BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

        VkBuffer indexBuffer;
        if (::vkCreateBuffer(device.raw_data(), &bufferInfo, nullptr, &indexBuffer) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create index buffer!");
        }

        VkMemoryRequirements memRequirements;
        ::vkGetBufferMemoryRequirements(device.raw_data(), indexBuffer, &memRequirements);

        // diff: [new] 设置内存分配标志
        VkMemoryAllocateFlagsInfo allocateFlagsInfo = {
            .sType = sType<VkMemoryAllocateFlagsInfo>(),
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};

        VkMemoryAllocateInfo allocInfo = {
            .sType = sType<VkMemoryAllocateInfo>(),
            .pNext = &allocateFlagsInfo,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                              VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT)};

        VkDeviceMemory indexBufferMemory;
        if (::vkAllocateMemory(device.raw_data(), &allocInfo, nullptr,
                               &indexBufferMemory) != VK_SUCCESS)
        {
            ::vkDestroyBuffer(device.raw_data(), indexBuffer, nullptr);
            throw std::runtime_error("failed to allocate index buffer memory!");
        }

        if (::vkBindBufferMemory(device.raw_data(), indexBuffer, indexBufferMemory, 0) !=
            VK_SUCCESS)
        {
            ::vkFreeMemory(device.raw_data(), indexBufferMemory, nullptr);
            ::vkDestroyBuffer(device.raw_data(), indexBuffer, nullptr);
            throw std::runtime_error("failed to bind index buffer memory!");
        }

        // 创建临时staging buffer来复制索引数据
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

        stagingBuffer.mapAndUnmapMemory(indices.data(), static_cast<size_t>(BUFFER_SIZE));

        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 indexBuffer, BUFFER_SIZE);

        // diff: [new] 将创建的缓冲区包装到buffer_base中
        fb.indexBuffer = mcs::vulkan::buffer_base(device, indexBuffer, indexBufferMemory);
    }

  public:
    // diff: [new] 获取当前帧的设备地址
    VkDeviceAddress getVertexBufferAddress(uint32_t currentFrame) const noexcept
    {
        return frameBuffers[currentFrame].vertexBufferAddress;
    }

    VkDeviceAddress getIndexBufferAddress(uint32_t currentFrame) const noexcept
    {
        return frameBuffers[currentFrame].indexBufferAddress;
    }

    // diff: [new]
    // 添加bindIndexBuffer方法，因为即使使用设备地址，Vulkan仍需要绑定索引缓冲区
    void bindIndexBuffer(VkCommandBuffer commandBuffer,
                         uint32_t currentFrame) const noexcept
    {
        ::vkCmdBindIndexBuffer(commandBuffer,
                               frameBuffers[currentFrame].indexBuffer.buffer(), 0,
                               VK_INDEX_TYPE_UINT32);
    }
};
// diff: end------------------------------------------------------------

// NOTE: 源自 test_vertex_input2.cpp。 使用 bufferDeviceAddress + bindless
int main()
{
    try
    {
        // diff: [new] 添加bufferDeviceAddress扩展
        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME}; // diff: [new] 添加设备地址扩展

        // diff: [new] 启用bufferDeviceAddress特性
        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceVulkan12Features, // diff: [new]
                                                          // 添加Vulkan 1.2特性
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
                .requiredFeatures([](const physical_device
                                         &physicalDevice) constexpr noexcept -> bool {
                    auto query =
                        structure_chain<VkPhysicalDeviceFeatures2,
                                        VkPhysicalDeviceVulkan13Features,
                                        VkPhysicalDeviceVulkan12Features,
                                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{
                            {}, {}, {}, {}};
                    physicalDevice.getFeatures2(query.head());
                    auto &features2 = query.template get<VkPhysicalDeviceFeatures2>();
                    auto &query_vulkan13_features =
                        query.template get<VkPhysicalDeviceVulkan13Features>();
                    auto &query_vulkan12_features = // diff: [new] 检查Vulkan 1.2特性
                        query.template get<VkPhysicalDeviceVulkan12Features>();
                    auto &query_extended_dynamic_state_features = query.template get<
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                    return features2.features.shaderInt64 &&
                           query_vulkan13_features.dynamicRendering &&
                           query_vulkan13_features.synchronization2 &&

                           query_vulkan12_features
                               .bufferDeviceAddress && // diff: [new]
                                                       // 检查Vulkan 1.2中的bufferDeviceAddress
                           query_vulkan12_features
                               .scalarBlockLayout && // diff: [new]
                                                     // 检查scalarBlockLayout

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

            // diff: [new] 使用bindless，移除传统的顶点输入描述
            // 设置为空，因为顶点数据将通过设备地址访问
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = nullptr};

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
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            // NOTE: 配置错误，配置不全，崩溃给你看。因此要非常小心
            //  auto msaaSamples = physicalDevice.getMaxUsableSampleCount();
            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // 没有硬件采样配置
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

            // diff: [new] 定义推送常量范围
            VkPushConstantRange pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(PushConstants) // 推送常量的大小
            };

            // diff: [new] 修改管道布局以包含推送常量
            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(),
                .setLayoutCount = 0,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushConstantRange};

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

        // diff: [new] 修改recordCommandBuffer以使用bindless
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame, mesh_data &input_mesh,
                                       uint32_t imageIndex) {
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

            // diff: [new] 推送设备地址到着色器
            PushConstants pushConstants = {
                .vertexBufferAddress = input_mesh.getVertexBufferAddress(currentFrame),
                .indexBufferAddress = input_mesh.getIndexBufferAddress(currentFrame)};

            ::vkCmdPushConstants(commandBuffer, pipelineLayout,
                                 VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants),
                                 &pushConstants);

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

            // diff: [new] 即使使用设备地址，Vulkan仍需要绑定索引缓冲区
            input_mesh.bindIndexBuffer(commandBuffer, currentFrame);

            // diff: [new] 使用设备地址绘制，不需要绑定顶点和索引缓冲区
            // 直接绘制索引化的几何体
            ::vkCmdDrawIndexed(commandBuffer,
                               static_cast<uint32_t>(input_mesh.indices.size()), 1, 0, 0,
                               0);

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

            // diff: vkResetFences 之后更新顶点. 仅仅更新飞行的帧
            input_mesh.applyQueuedUpdate(currentFrame);
            recordCommandBuffer(commandBuffer, currentFrame, input_mesh, imageIndex);

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
                    // 切换到三角形 // NOLINTBEGIN
                    static const std::vector<Vertex> VERTICES = {
                        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                        {{0.0f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
                    static const std::vector<uint32_t> INDICES = {0, 1, 2}; // NOLINTEND
                    input_mesh.queueUpdate(VERTICES, INDICES);
                }
                else
                {
                    // 只记录要更新的数据，不立即执行
                    input_mesh.queueUpdate(vertices, indices);
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