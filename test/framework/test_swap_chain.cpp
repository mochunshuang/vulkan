#include "./head.hpp"

#include <concepts>
#include <exception>
#include <iostream>
#include <print>
#include <utility>

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

        swapChain_ = swapChainBuild_.build();
        colorImage_ = colorImageBuild_.build();
        deepImage_ = deepImageBuild_.build();
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

int main()
{
    // mcs::vulkan::wsi::glfw::Window
    try
    {

        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}; // NOLINTEND

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan11Features,
                        VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                // C9:  检查是否支持各向异性采样。纹理映射需要
                {.features = {.samplerAnisotropy = VK_TRUE}},
                {.shaderDrawParameters = VK_TRUE},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

        surface window{};
        window.setup({.width = 800, .height = 600}, "test"); // NOLINT
        mcs::vulkan::context_base ctx;
        ctx.createInstance(
               make_instance{}
                   .enableDebugExtension()
                   .enableSurfaceExtension<surface>()
                   .checkExtensionSupport()
                   .checkLayerSupport()
                   .build({.sType = sType<VkApplicationInfo>(),
                           .pApplicationName = "Hello Triangle",
                           .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                           .pEngineName = "No Engine",
                           .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                           // apiVersion必须是应用程序设计使用的Vulkan的最高版本
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
                    .afterQueueCreateInfoInit([]([[maybe_unused]] VkDeviceQueueCreateInfo
                                                     &queueCreateInfo) noexcept {})
                    .afterBuildSuccess([&](const logical_device &logicalDevice,
                                           uint32_t queueFamilyIndex) {
                        // use queueFamilyIndex
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

        context_wsi<surface> context_with_surface{ctx, window};
        {
            // NOTE: 上下文对象存放 surface 应该会好一些。那就得分得更细。这样也好
            swap_chain swap_chain =
                make_swap_chain<context_wsi<surface>>{context_with_surface}
                    .requiredSwapchainCreateInfoKHR(
                        [](make_swap_chain<context_wsi<surface>> *self)
                            -> VkSwapchainCreateInfoKHR {
                            auto *ctx = self->context();
                            auto imageExtent = ctx->surfaceExtent();

                            auto swapChainSurfaceFormat = ctx->chooseSurfaceFormat(
                                {.format = VK_FORMAT_B8G8R8A8_SRGB,
                                 .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

                            auto minImageCount = ctx->minImageCount();

                            // chooseSwapPresentMode
                            auto presentMode =
                                ctx->choosePresentMode(VK_PRESENT_MODE_MAILBOX_KHR);

                            auto surfaceCapabilities = ctx->getSurfaceCapabilitiesKHR();
                            return {.sType = sType<VkSwapchainCreateInfoKHR>(),
                                    .surface = ctx->surface(),
                                    .minImageCount = minImageCount,
                                    .imageFormat = swapChainSurfaceFormat.format,
                                    .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                                    .imageExtent = imageExtent,
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
                            return {.sType = sType<VkImageViewCreateInfo>(),
                                    .image = image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = imageCreateInfo.imageFormat,
                                    .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                   .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                   .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                   .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                                    .subresourceRange = {.aspectMask =
                                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                                         .baseMipLevel = 0,
                                                         .levelCount = 1,
                                                         .baseArrayLayer = 0,
                                                         .layerCount = 1}};
                        })
                    .build();
            mcs::MCS_ASSERT(swap_chain.valid());

            [[maybe_unused]] auto b = std::move(swap_chain);

            mcs::MCS_ASSERT(not swap_chain.valid());

            std::cout << "sizeof(swap_chain): " << sizeof(swap_chain) << '\n';

            std::cout << "sizeof(make_swap_chain<surface>): "
                      << sizeof(make_swap_chain<surface>) << '\n';
        }

        {
            my_swap_chain<context_wsi<surface>> my_swapchain = {
                // 1. swap_chain
                make_swap_chain<context_wsi<surface>>{context_with_surface}
                    .requiredSwapchainCreateInfoKHR(
                        [](make_swap_chain<context_wsi<surface>> *self)
                            -> VkSwapchainCreateInfoKHR {
                            auto *ctx = self->context();
                            auto imageExtent = ctx->surfaceExtent();

                            auto swapChainSurfaceFormat = ctx->chooseSurfaceFormat(
                                {.format = VK_FORMAT_B8G8R8A8_SRGB,
                                 .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

                            auto minImageCount = ctx->minImageCount();

                            // chooseSwapPresentMode
                            auto presentMode =
                                ctx->choosePresentMode(VK_PRESENT_MODE_MAILBOX_KHR);

                            auto surfaceCapabilities = ctx->getSurfaceCapabilitiesKHR();
                            return {.sType = sType<VkSwapchainCreateInfoKHR>(),
                                    .surface = ctx->surface(),
                                    .minImageCount = minImageCount,
                                    .imageFormat = swapChainSurfaceFormat.format,
                                    .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                                    .imageExtent = imageExtent,
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
                            return {.sType = sType<VkImageViewCreateInfo>(),
                                    .image = image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = imageCreateInfo.imageFormat,
                                    .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                   .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                   .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                   .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                                    .subresourceRange = {.aspectMask =
                                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                                         .baseMipLevel = 0,
                                                         .levelCount = 1,
                                                         .baseArrayLayer = 0,
                                                         .layerCount = 1}};
                        }),
                // 2. color_image
                make_color_image<context_wsi<surface>>{context_with_surface}
                    .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    .requiredImageCreateInfo([](make_color_image<context_wsi<surface>>
                                                    *self) -> VkImageCreateInfo {
                        auto surfaceFormats = self->context()->getSurfaceFormatsKHR();
                        auto msaaSamples = self->context()->getMaxUsableSampleCount();

                        // NOTE: 得到实时窗口大小，非常关键
                        auto swapChainExtent = self->context()->surfaceExtent();

                        auto swapChainSurfaceFormat =
                            self->context()->chooseSurfaceFormat(
                                {.format = VK_FORMAT_B8G8R8A8_SRGB,
                                 .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

                        auto colorFormat = swapChainSurfaceFormat.format;

                        return {
                            .sType = sType<VkImageCreateInfo>(),
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
                            return {
                                .sType = sType<VkImageViewCreateInfo>(),
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
                // 3. deep_image
                make_deep_image<context_wsi<surface>>{context_with_surface}
                    .requiredFormatProperties(
                        [](VkFormatProperties props) noexcept -> bool {
                            constexpr auto REQUIRED_TILING =
                                VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
                            constexpr auto REQUIRED_FEATURES = VkFormatFeatureFlagBits::
                                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
                            return make_deep_image<surface>::selectDeepFormat(
                                props, REQUIRED_TILING, REQUIRED_FEATURES);
                        })
                    .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    .requiredImageCreateInfo([](make_deep_image<context_wsi<surface>>
                                                    *self) -> VkImageCreateInfo {
                        // NOTE: 得到实时窗口大小，非常关键
                        auto swapChainExtent = self->context()->surfaceExtent();
                        VkFormat depthFormat = self->findDepthFormat(
                            {VkFormat::VK_FORMAT_D32_SFLOAT,
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
                                .initialLayout =
                                    VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED};
                    })
                    .requiredImageViewCreateInfo(
                        [](const VkImageCreateInfo &imageCreateInfo,
                           const VkImage &image) noexcept -> VkImageViewCreateInfo {
                            return {.sType = sType<VkImageViewCreateInfo>(),
                                    .image = image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = imageCreateInfo.format,
                                    .subresourceRange = {.aspectMask =
                                                             VK_IMAGE_ASPECT_DEPTH_BIT,
                                                         .baseMipLevel = 0,
                                                         .levelCount = 1,
                                                         .baseArrayLayer = 0,
                                                         .layerCount = 1}};
                        })};

            mcs::MCS_ASSERT(my_swapchain.valid());

            static_assert(not std::copyable<decltype(my_swapchain)>);

            auto temp = std::move(my_swapchain);
            mcs::MCS_ASSERT(not my_swapchain.valid());

            mcs::MCS_ASSERT(temp.valid());

            temp.recreateSwapChain();
            mcs::MCS_ASSERT(temp.valid());

            std::cout << "sizeof(my_swapchain): " << sizeof(my_swapchain) << '\n';
        }

        while (window.shouldClose() == 0)
        {
            surface::pollEvents();

            // 游戏逻辑...
            // NOTE: 黑色背景需要vulkan渲染
            if (window.framebufferResized())
            {
                std::cout << "Framebuffer resized!" << '\n';
                window.refFramebufferResized() = false;
            }
        }
        // window.teardown(); //NOTE: auto do that.
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "main done\n";
    return 0;
}