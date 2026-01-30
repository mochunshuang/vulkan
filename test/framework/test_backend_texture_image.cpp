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
#include "backend/surface_impl.hpp"
#include "backend/swap_chain_interface.hpp"
#include "backend/tools/simple_copy_buffer.hpp"
#include "backend/tools/staging_buffer.hpp"
#include "backend/wsi/glfw.hpp"
#include "backend/pick_physical_device.hpp"
#include "backend/create_logical_device.hpp"
#include "backend/sType.hpp"
#include "backend/select_queue_family_index.hpp"
#include "backend/structure_chain.hpp"
#include "backend/CommandPool.hpp"

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

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "test_my_triangle";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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

// NOTE: 绘制一个三角形 通过描述符
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

using buffer_base = mcs::vulkan::core::Queue;

using mcs::vulkan::tools::simple_copy_buffer;
using mcs::vulkan::tools::staging_buffer;
using mcs::vulkan::tools::create_buffer;

// diff: start
//  顶点结构体
struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord; // 添加纹理坐标

    static constexpr std::array<VkVertexInputBindingDescription, 1>
    getBindingDescriptions()
    {
        return {{{.binding = 0,
                  .stride = sizeof(Vertex),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}}};
    }

    static constexpr std::array<VkVertexInputAttributeDescription, 3> // 改为3个
    getAttributeDescriptions()
    {
        return {{// 位置属性
                 {.location = 0,
                  .binding = 0,
                  .format = VK_FORMAT_R32G32_SFLOAT,
                  .offset = offsetof(Vertex, pos)},
                 // 颜色属性
                 {.location = 1,
                  .binding = 0,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, color)},
                 // 纹理坐标属性
                 {.location = 2,
                  .binding = 0,
                  .format = VK_FORMAT_R32G32_SFLOAT,
                  .offset = offsetof(Vertex, texCoord)}}};
    }
};

// 顶点数据（四边形由两个三角形组成）
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 左下
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // 右下
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},   // 右上
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}   // 左上
};

// 索引数据
const std::vector<uint16_t> indices = {
    0, 1, 2, // 第一个三角形
    0, 2, 3  // 第二个三角形
};
class SimpleVertexBuffer
{
  private:
    mcs::vulkan::core::buffer_base vertexBuffer_;
    mcs::vulkan::core::buffer_base indexBuffer_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;

  public:
    SimpleVertexBuffer() = default;

    SimpleVertexBuffer(const CommandPool &pool, const Queue &queue,
                       const std::vector<Vertex> &vertices,
                       const std::vector<uint16_t> &indices)
        : vertexCount_(static_cast<uint32_t>(vertices.size())),
          indexCount_(static_cast<uint32_t>(indices.size()))
    {

        const LogicalDevice *device = pool.logicalDevice();

        // 创建顶点缓冲区
        if (vertexCount_ > 0)
        {
            VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

            // 使用暂存缓冲区
            auto stagingBuffer = staging_buffer(*device, bufferSize);
            stagingBuffer.copyDataToBuffer(vertices.data(), bufferSize);

            vertexBuffer_ = create_buffer(*device,
                                          structure_chain<VkBufferCreateInfo>{
                                              {.size = bufferSize,
                                               .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                               .sharingMode = VK_SHARING_MODE_EXCLUSIVE}},
                                          structure_chain<VkMemoryAllocateInfo>{{}},
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            simple_copy_buffer(pool, queue, stagingBuffer.buffer(),
                               vertexBuffer_.buffer(),
                               {VkBufferCopy{.size = bufferSize}});
        }

        // 创建索引缓冲区
        if (indexCount_ > 0)
        {
            VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

            auto stagingBuffer = staging_buffer(*device, bufferSize);
            stagingBuffer.copyDataToBuffer(indices.data(), bufferSize);

            indexBuffer_ = create_buffer(*device,
                                         structure_chain<VkBufferCreateInfo>{
                                             {.size = bufferSize,
                                              .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE}},
                                         structure_chain<VkMemoryAllocateInfo>{{}},
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            simple_copy_buffer(pool, queue, stagingBuffer.buffer(), indexBuffer_.buffer(),
                               {VkBufferCopy{.size = bufferSize}});
        }
    }

    void bind(VkCommandBuffer commandBuffer) const
    {
        if (vertexCount_ > 0)
        {
            VkBuffer vertexBuffers[] = {vertexBuffer_.buffer()};
            VkDeviceSize offsets[] = {0};
            ::vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        }

        if (indexCount_ > 0)
        {
            ::vkCmdBindIndexBuffer(commandBuffer, indexBuffer_.buffer(), 0,
                                   VK_INDEX_TYPE_UINT16);
        }
    }

    [[nodiscard]] uint32_t getIndexCount() const
    {
        return indexCount_;
    }
    [[nodiscard]] uint32_t getVertexCount() const
    {
        return vertexCount_;
    }

    void clear()
    {
        vertexBuffer_ = mcs::vulkan::core::buffer_base{};
        indexBuffer_ = mcs::vulkan::core::buffer_base{};
    }
};

#include <stb_image.h>

class TextureImage
{
  private:
    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;

    int texWidth_ = 0;
    int texHeight_ = 0;
    int texChannels_ = 0;

    const LogicalDevice *device_ = nullptr;

  public:
    TextureImage() = default;

    TextureImage(const LogicalDevice &device, const CommandPool &pool, const Queue &queue,
                 const std::string &path)
        : device_(&device)
    {
        // 加载图片
        stbi_uc *pixels = stbi_load(path.c_str(), &texWidth_, &texHeight_, &texChannels_,
                                    STBI_rgb_alpha);
        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }

        VkDeviceSize imageSize = texWidth_ * texHeight_ * 4; // RGBA

        // 创建暂存缓冲区
        auto stagingBuffer = staging_buffer(device, imageSize);
        stagingBuffer.copyDataToBuffer(pixels, imageSize);

        stbi_image_free(pixels);

        // 创建图像
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = static_cast<uint32_t>(texWidth_);
        imageInfo.extent.height = static_cast<uint32_t>(texHeight_);
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;

        textureImage_ = device_->createImage(imageInfo, device_->allocator());

        // 分配内存
        VkMemoryRequirements memRequirements =
            device_->getImageMemoryRequirements(textureImage_);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = device.findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        textureImageMemory_ = device_->allocateMemory(allocInfo, device_->allocator());

        device_->bindImageMemory(textureImage_, textureImageMemory_, 0);

        // 转换图像布局并复制数据
        auto commandBuffer = pool.allocateOneCommandBuffer(
            {.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1});

        commandBuffer.begin({});

        // 转换布局为传输目标
        my_render::transition_image_layout(
            commandBuffer, textureImage_, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {},
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        // 复制缓冲区到图像
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {static_cast<uint32_t>(texWidth_),
                              static_cast<uint32_t>(texHeight_), 1};

        commandBuffer.copyBufferToImage(stagingBuffer.buffer(), textureImage_,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // 转换布局为着色器读取
        my_render::transition_image_layout(
            commandBuffer, textureImage_, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        commandBuffer.end();

        // 提交命令
        queue.submit(1,
                     {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                      .commandBufferCount = 1,
                      .pCommandBuffers = &*commandBuffer},
                     nullptr);
        queue.waitIdle();

        // 创建图像视图
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        textureImageView_ = device_->createImageView(viewInfo, device_->allocator());

        // 创建采样器
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;

        VkPhysicalDeviceProperties properties =
            device_->physicalDevice()->getProperties();
        samplerInfo.maxAnisotropy =
            properties.limits.maxSamplerAnisotropy; // NOTE: 需要启动特性

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        textureSampler_ = device_->createSampler(samplerInfo, device_->allocator());
    }

    void clear() noexcept
    {
        if (textureSampler_ != VK_NULL_HANDLE)
        {
            device_->destroySampler(textureSampler_, device_->allocator());
            textureSampler_ = VK_NULL_HANDLE;
        }
        if (textureImageView_ != VK_NULL_HANDLE)
        {
            device_->destroyImageView(textureImageView_, device_->allocator());
            textureImageView_ = VK_NULL_HANDLE;
        }
        if (textureImage_ != VK_NULL_HANDLE)
        {
            device_->destroyImage(textureImage_, device_->allocator());
            textureImage_ = VK_NULL_HANDLE;
        }
        if (textureImageMemory_ != VK_NULL_HANDLE)
        {
            device_->freeMemory(textureImageMemory_, device_->allocator());
            textureImageMemory_ = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkImageView imageView() const noexcept
    {
        return textureImageView_;
    }
    [[nodiscard]] VkSampler sampler() const noexcept
    {
        return textureSampler_;
    }
};

constexpr auto VERT_SHADER_PATH = "shaders/test_backend_texture_image_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_backend_texture_image_frag.spv";
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

    structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
        enablefeatureChain = {
            {.features = {.samplerAnisotropy = VK_TRUE}}, // diff: 纹理启用各向异性过滤
            {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
            {.extendedDynamicState = VK_TRUE}};

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
                .requiredFeatures(
                    [](const PhysicalDevice &physicalDevice) constexpr noexcept -> bool {
                        auto query = structure_chain<
                            VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{{}, {}, {}};
                        physicalDevice.getFeatures2(&query.head());
                        auto &query_features2 =
                            query.template get<VkPhysicalDeviceFeatures2>().features;
                        auto &query_vulkan13_features =
                            query.template get<VkPhysicalDeviceVulkan13Features>();
                        auto &query_extended_dynamic_state_features = query.template get<
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                        return query_features2
                                   .samplerAnisotropy && // diff: 新开启的特性要验证是否OK
                               query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
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

    // diff: start
    auto commandPool =
        CommandPool{ctx.logicalDevice(),
                    {.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                     .queueFamilyIndex = GRAPHICS_QUEUE_FAMILY_IDX}};
    const std::string TEXTURE_PATH = "textures/texture.jpg";
    TextureImage textureImage(ctx.logicalDevice(), commandPool, graphicsAndPresentQueue,
                              TEXTURE_PATH);

    // 创建描述符集布局
    VkDescriptorSetLayoutBinding samplerLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &samplerLayoutBinding};
    VkDescriptorSetLayout descriptorSetLayout =
        ctx.logicalDevice().createDescriptorSetLayout(layoutInfo,
                                                      ctx.logicalDevice().allocator());

    // 创建描述符池
    VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .descriptorCount =
                                      static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize};
    VkDescriptorPool descriptorPool = ctx.logicalDevice().createDescriptorPool(
        poolInfo, ctx.logicalDevice().allocator());

    // 创建描述符集
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layouts.data()};
    std::vector<VkDescriptorSet> descriptorSets =
        ctx.logicalDevice().allocateDescriptorSets(allocInfo);

    // 更新描述符集
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo{.sampler = textureImage.sampler(),
                                        .imageView = textureImage.imageView(),
                                        .imageLayout =
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo};
        ctx.logicalDevice().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
    }

    // NOTE: pipeline_layout 和 graphics_pipeline 可以是多对多的关系
    const auto LAYOUT_ID =
        ctx.addPipelineLayout(create_pipeline_layout{ctx.logicalDevice()}
                                  .setLayouts({descriptorSetLayout}) // diff: 纹理需要
                                  .pushConstantRanges({})
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
            // diff: 顶点需要配置顶点输入,纹理也配置在这里
            .configVertexInputState(
                {.vertexBindingDescriptions =
                     Vertex::getBindingDescriptions() | std::ranges::to<std::vector>(),
                 .vertexAttributeDescriptions =
                     Vertex::getAttributeDescriptions() | std::ranges::to<std::vector>()})
            .configAssemblyState({.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                  .primitiveRestartEnable = VK_FALSE})
            // NOTE: 使用动态渲染初始值随意
            .configViewportState({.viewports = {VkViewport{}}, .scissors = {VkRect2D{}}})
            .configRasterizationState({.depthClampEnable = VK_FALSE,
                                       .rasterizerDiscardEnable = VK_FALSE,
                                       .polygonMode = VK_POLYGON_MODE_FILL,
                                       .cullMode = VK_CULL_MODE_BACK_BIT,
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

    std::vector<CommandBuffer> commandBuffers =
        commandPool.allocateCommandBuffers({.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                            .commandBufferCount = MAX_FRAMES_IN_FLIGHT});

    frame_context frameContext{MAX_FRAMES_IN_FLIGHT, ctx.logicalDevice(),
                               swapChain.swapChainImagesSize()};

    //  创建顶点缓冲区
    SimpleVertexBuffer vertexBuffer(commandPool, graphicsAndPresentQueue, vertices,
                                    indices);
    // diff: end

    // NOLINTNEXTLINE
    const auto recordCommandBuffer = [&](const CommandBuffer &commandBuffer,
                                         VkDescriptorSet descriptorSet,
                                         uint32_t imageIndex) {
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

        // diff: start 绑定顶点和索引缓冲区,以及描述符
        {
            vertexBuffer.bind(*commandBuffer);
            // 绑定描述符集
            vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    ctx.getPipelineLayout(LAYOUT_ID), 0, 1,
                                    &descriptorSet, 0, nullptr);
            // 绘制（使用索引）
            ::vkCmdDrawIndexed(*commandBuffer, vertexBuffer.getIndexCount(), 1, 0, 0, 0);
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
        // diff: start
        recordCommandBuffer(commandBuffer, descriptorSets[currentFrame], imageIndex);
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

    // diff: 需要清理资源
    vertexBuffer.clear();
    textureImage.clear();

    vkDestroyDescriptorPool(*ctx.logicalDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(*ctx.logicalDevice(), descriptorSetLayout, nullptr);
    // diff: end

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