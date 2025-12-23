#include "./head.hpp"

#include <exception>
#include <iostream>
#include <print>

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

        using mcs::vulkan::structure_chain;
        using mcs::vulkan::sType;

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