#include "./head.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <exception>
#include <iostream>
#include <print>
#include <utility>

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
        // inFlightFences.clear();
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
                                    .apiVersion = VkApiVersion(0, 1, 4, 0)});

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
            mcs::vulkan::shader_module vertshader{
                logical_device_, "shaders/test_drawIndexed_vert.spv"}; // diff: 修改着色器
            mcs::vulkan::shader_module fragshader{logical_device_,
                                                  "shaders/test_triangle_frag.spv"};
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
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
            };

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
        // diff: start: 创建复杂图案的索引数据. 最多21个
        std::vector<uint32_t> indices = {0, 1,
                                         2,       // 第一个三角形
                                         3, 4, 5, // 第二个三角形
                                         6, 7, 8, 9, 10, 11, 12, 13, 14, 14, 15, 16};

        // 创建索引缓冲区
        VkBuffer indexBuffer = nullptr;
        VkDeviceMemory indexBufferMemory = nullptr;
        VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

        {
            // 创建暂存缓冲区
            VkBuffer stagingBuffer = nullptr;
            VkDeviceMemory stagingBufferMemory = nullptr;

            VkBufferCreateInfo bufferInfo = {.sType = sType<VkBufferCreateInfo>(),
                                             .size = indexBufferSize,
                                             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

            stagingBuffer = logical_device_.createBuffer(bufferInfo, nullptr);

            VkMemoryRequirements memRequirements =
                logical_device_.getBufferMemoryRequirements(stagingBuffer);

            // 找到适合的主机可见内存
            uint32_t memoryTypeIndex = physicalDevice.findMemoryType(
                memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VkMemoryAllocateInfo allocInfo = {.sType = sType<VkMemoryAllocateInfo>(),
                                              .allocationSize = memRequirements.size,
                                              .memoryTypeIndex = memoryTypeIndex};

            stagingBufferMemory = logical_device_.allocateMemory(allocInfo, nullptr);
            logical_device_.bindBufferMemory(stagingBuffer, stagingBufferMemory, 0);

            // 复制数据到暂存缓冲区
            void *data = nullptr;
            logical_device_.mapMemory(stagingBufferMemory, 0, indexBufferSize, 0, &data);
            memcpy(data, indices.data(), (size_t)indexBufferSize);
            logical_device_.unmapMemory(stagingBufferMemory);

            // 创建设备本地索引缓冲区
            bufferInfo.usage =
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            indexBuffer = logical_device_.createBuffer(bufferInfo, nullptr);

            memRequirements = logical_device_.getBufferMemoryRequirements(indexBuffer);

            // 找到适合的设备本地内存
            memoryTypeIndex = physicalDevice.findMemoryType(
                memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = memoryTypeIndex;

            indexBufferMemory = logical_device_.allocateMemory(allocInfo, nullptr);
            logical_device_.bindBufferMemory(indexBuffer, indexBufferMemory, 0);

            // 开始命令缓冲区录制
            VkCommandBufferBeginInfo beginInfo = {
                .sType = sType<VkCommandBufferBeginInfo>(),
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

            // 创建命令缓冲区用于传输
            VkCommandBuffer transferCommandBuffer = nullptr;
            logical_device_.allocateCommandBuffers(
                transferCommandBuffer, {.sType = sType<VkCommandBufferAllocateInfo>(),
                                        .commandPool = commandPool,
                                        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        .commandBufferCount = 1});
            ::vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

            // 复制缓冲区
            VkBufferCopy copyRegion = {
                .srcOffset = 0, .dstOffset = 0, .size = indexBufferSize};
            ::vkCmdCopyBuffer(transferCommandBuffer, stagingBuffer, indexBuffer, 1,
                              &copyRegion);

            ::vkEndCommandBuffer(transferCommandBuffer);

            // 提交并执行复制命令
            VkSubmitInfo submitInfo = {.sType = sType<VkSubmitInfo>(),
                                       .commandBufferCount = 1,
                                       .pCommandBuffers = &transferCommandBuffer};

            ::vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            ::vkQueueWaitIdle(graphicsQueue);

            // 清理暂存资源
            logical_device_.destroyBuffer(stagingBuffer, nullptr);
            logical_device_.freeMemory(stagingBufferMemory, nullptr);

            // 释放索引缓冲区资源
            if (transferCommandBuffer != nullptr)
            {
                logical_device_.freeCommandBuffers(commandPool, 1, transferCommandBuffer);
                transferCommandBuffer = nullptr;
            }
        }
        // diff: end

        // c0: 14. recordCommandBuffer
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
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

            // diff: start 改成索引绘制
            // 绑定索引缓冲区
            ::vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // 使用索引绘制
            ::vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0,
                               0, 0);
            // diff: end

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
            recordCommandBuffer(commandBuffer, imageIndex);

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
            drawFrame();
        }
        //  c0: 18. destroy. 暂时不知道如何封装。或许
        // NOTE: 9. Don't release anything until the GPU is completely idle.
        ::vkDeviceWaitIdle(logical_device_.raw_data());

        // diff: start

        if (indexBuffer != nullptr)
        {
            logical_device_.destroyBuffer(indexBuffer, nullptr);
            indexBuffer = nullptr;
        }

        if (indexBufferMemory != nullptr)
        {
            logical_device_.freeMemory(indexBufferMemory, nullptr);
            indexBufferMemory = nullptr;
        }
        // diff: end

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

// NOTE: 这是最最基础的 三角形绘制代码