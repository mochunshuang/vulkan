#include "./head.hpp"

#include <exception>
#include <iostream>
#include <print>
#include <utility>

int main()
{
    // mcs::vulkan::wsi::glfw::Window
    try
    {
        using surface = mcs::vulkan::wsi::glfw::Window;

        using make_physical_device = mcs::vulkan::make_physical_device;
        using make_instance = mcs::vulkan::make_instance;
        using physical_device = mcs::vulkan::physical_device;
        using make_logical_device = mcs::vulkan::make_logical_device;
        using make_queue_family_index = mcs::vulkan::make_queue_family_index;

        using logical_device = mcs::vulkan::logical_device;

        using mcs::vulkan::make_swap_chain;
        using swap_chain [[maybe_unused]] = mcs::vulkan::swap_chain;

        using mcs::vulkan::make_color_image;
        using mcs::vulkan::make_deep_image;
        using mcs::vulkan::make_texture_image;

        using mcs::vulkan::structure_chain;
        using mcs::vulkan::sType;

        using mcs::vulkan::context_wsi;

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
                        std::cout << ">>>>> queueFamilyIndex : " << queueFamilyIndex
                                  << '\n';
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
            auto color_image =
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
                        })
                    .build();

            mcs::MCS_ASSERT(color_image.valid());
            [[maybe_unused]] auto d = std::move(color_image);
            mcs::MCS_ASSERT(not color_image.valid());

            std::cout << ">>> sizeof(color_image): " << sizeof(color_image) << '\n';
            std::cout << ">>> sizeof(make_color_image<surface>): "
                      << sizeof(make_color_image<surface>) << '\n';
        }

        {
            auto deep_image =
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
                        })
                    .build();

            mcs::MCS_ASSERT(deep_image.valid());
            [[maybe_unused]] auto d = std::move(deep_image);
            mcs::MCS_ASSERT(not deep_image.valid());

            std::cout << ">>> sizeof(deep_image): " << sizeof(deep_image) << '\n';
            std::cout << ">>> sizeof(make_deep_image<surface>): "
                      << sizeof(make_deep_image<surface>) << '\n';
        }

        // 在选择队列族之前，打印所有队列族的信息
        auto queueFamilies =
            ctx.ref_physical_device().getQueueFamilyProperties(); // NOLINTBEGIN
        std::cout << "Available queue families:\n";
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            auto flags = queueFamilies[i].queueFlags;
            std::cout << "  Queue family " << i << ": ";
            if (flags & VK_QUEUE_GRAPHICS_BIT)
                std::cout << "GRAPHICS ";
            if (flags & VK_QUEUE_COMPUTE_BIT)
                std::cout << "COMPUTE ";
            if (flags & VK_QUEUE_TRANSFER_BIT)
                std::cout << "TRANSFER ";
            if (flags & VK_QUEUE_SPARSE_BINDING_BIT)
                std::cout << "SPARSE_BINDING ";

            // 检查表面支持
            VkBool32 presentSupport =
                ctx.ref_physical_device().getSurfaceSupportKHR(i, ctx.surface());
            std::cout << "| Surface support: " << ((presentSupport != 0U) ? "YES" : "NO");
            std::cout << '\n';
        } // NOLINTEND

        const std::string TEXTURE_PATH = "textures/texture.jpg";
        {
            auto texture_image =
                make_texture_image<context_wsi<surface>>{context_with_surface}
                    .setFilePath(TEXTURE_PATH)
                    .setFlip(false)
                    .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    .requiredImageCreateInfo([](make_texture_image<context_wsi<surface>>
                                                    *self) -> VkImageCreateInfo {
                        const auto &raw_image = self->rawImage();
                        return {
                            .sType = sType<VkImageCreateInfo>(),
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = VK_FORMAT_R8G8B8A8_SRGB,
                            .extent = {.width = static_cast<uint32_t>(raw_image.width()),
                                       .height =
                                           static_cast<uint32_t>(raw_image.height()),
                                       .depth = 1},
                            .mipLevels = raw_image.mipLevels(), // c14: 应用mipLevels
                            .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                            // c14: 添加VK_IMAGE_USAGE_TRANSFER_SRC_BIT 用于mipmap生成
                            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                     VK_IMAGE_USAGE_SAMPLED_BIT,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
                        ;
                    })
                    .requiredImageViewCreateInfo(
                        [](const VkImageCreateInfo &imageCreateInfo,
                           const VkImage &image) noexcept -> VkImageViewCreateInfo {
                            return {.sType = sType<VkImageViewCreateInfo>(),
                                    .image = image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = imageCreateInfo.format,
                                    .subresourceRange = {
                                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                        .baseMipLevel = 0,
                                        .levelCount =
                                            imageCreateInfo.mipLevels, // c14:mipLevels
                                        .baseArrayLayer = 0,
                                        .layerCount = 1}};
                        })
                    .requiredSamplerCreateInfo([](make_texture_image<context_wsi<surface>>
                                                      *self) -> VkSamplerCreateInfo {
                        VkPhysicalDeviceProperties properties =
                            self->context()->ref_physical_device().getProperties();

                        return {.sType = sType<VkSamplerCreateInfo>(),
                                .magFilter = VkFilter::VK_FILTER_LINEAR,
                                .minFilter = VkFilter::VK_FILTER_LINEAR,
                                .mipmapMode =
                                    VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                .addressModeU =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .addressModeV =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .addressModeW =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .mipLodBias = 0.0F,
                                // NOTE: 4. 各向异性器件特性启用
                                .anisotropyEnable = VK_TRUE,
                                .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                                .compareEnable = VK_FALSE,
                                .compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS,
                                .minLod = 0.0F,
                                .maxLod = 0.0F,
                                .borderColor = {},
                                .unnormalizedCoordinates = VK_FALSE};
                    })
                    .build();

            mcs::MCS_ASSERT(texture_image.valid());
            [[maybe_unused]] auto d = std::move(texture_image);
            mcs::MCS_ASSERT(not texture_image.valid());

            std::cout << ">>> sizeof(texture_image): " << sizeof(texture_image) << '\n';
            std::cout << ">>> sizeof(make_texture_image): "
                      << sizeof(make_texture_image<context_wsi<surface>>) << '\n';
        }
        // 链式调用返回是引用，需要move
        {
            auto texture_image_build = std::move(
                make_texture_image<context_wsi<surface>>{context_with_surface}
                    .setFilePath(TEXTURE_PATH)
                    .setFlip(false)
                    .requiredMemoryProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    .requiredImageCreateInfo([](make_texture_image<context_wsi<surface>>
                                                    *self) -> VkImageCreateInfo {
                        const auto &raw_image = self->rawImage();
                        return {
                            .sType = sType<VkImageCreateInfo>(),
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = VK_FORMAT_R8G8B8A8_SRGB,
                            .extent = {.width = static_cast<uint32_t>(raw_image.width()),
                                       .height =
                                           static_cast<uint32_t>(raw_image.height()),
                                       .depth = 1},
                            .mipLevels = raw_image.mipLevels(), // c14: 应用mipLevels
                            .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                            // c14: 添加VK_IMAGE_USAGE_TRANSFER_SRC_BIT 用于mipmap生成
                            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                     VK_IMAGE_USAGE_SAMPLED_BIT,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
                        ;
                    })
                    .requiredImageViewCreateInfo(
                        [](const VkImageCreateInfo &imageCreateInfo,
                           const VkImage &image) noexcept -> VkImageViewCreateInfo {
                            return {.sType = sType<VkImageViewCreateInfo>(),
                                    .image = image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = imageCreateInfo.format,
                                    .subresourceRange = {
                                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                        .baseMipLevel = 0,
                                        .levelCount =
                                            imageCreateInfo.mipLevels, // c14:mipLevels
                                        .baseArrayLayer = 0,
                                        .layerCount = 1}};
                        })
                    .requiredSamplerCreateInfo([](make_texture_image<context_wsi<surface>>
                                                      *self) -> VkSamplerCreateInfo {
                        VkPhysicalDeviceProperties properties =
                            self->context()->ref_physical_device().getProperties();

                        return {.sType = sType<VkSamplerCreateInfo>(),
                                .magFilter = VkFilter::VK_FILTER_LINEAR,
                                .minFilter = VkFilter::VK_FILTER_LINEAR,
                                .mipmapMode =
                                    VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                .addressModeU =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .addressModeV =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .addressModeW =
                                    VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                .mipLodBias = 0.0F,
                                // NOTE: 4. 各向异性器件特性启用
                                .anisotropyEnable = VK_TRUE,
                                .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                                .compareEnable = VK_FALSE,
                                .compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS,
                                .minLod = 0.0F,
                                .maxLod = 0.0F,
                                .borderColor = {},
                                .unnormalizedCoordinates = VK_FALSE};
                    }));
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