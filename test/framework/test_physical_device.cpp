#include "./head.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <limits>
#include <print>
#include <type_traits>

static void base() // NOLINT
{
    try
    {
        using surface = mcs::vulkan::wsi::glfw::Window;

        surface window{};
        window.setup({.width = 800, .height = 600}, "test"); // NOLINT
        mcs::vulkan::context_base ctx;
        ctx.createInstance(
               mcs::vulkan::make_instance{}
                   .enableDebugExtension()
                   .enableSurfaceExtension<surface>()
                   .checkExtensionSupport()
                   .checkLayerSupport()
                   .build(mcs::vulkan::make_instance::defaultApplicationInfo()))
            .createSurface(window);

        const auto &instance = ctx.ref_instance();

        mcs::MCS_ASSERT(instance.valid());
        mcs::MCS_ASSERT(instance.isEnableDebugExtension());

        uint32_t gpu_count = 0;
        mcs::vulkan::utils::check_vk_result(
            vkEnumeratePhysicalDevices(ctx.raw_instance(), &gpu_count, nullptr));
        mcs::MCS_ASSERT(gpu_count > 0);
        std::vector<VkPhysicalDevice> gpus(gpu_count);
        mcs::vulkan::utils::check_vk_result(
            vkEnumeratePhysicalDevices(ctx.raw_instance(), &gpu_count, gpus.data()));

        constexpr auto UNIT32_MAX = std::numeric_limits<uint32_t>::max();

        uint32_t graphics_queue_index{UNIT32_MAX};
        VkPhysicalDevice gpu = VK_NULL_HANDLE;
        for (auto *surface = ctx.surface(); const auto &physical_device : gpus)
        { // Check if the device supports Vulkan 1.3
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(physical_device, &device_properties);
            mcs::MCS_ASSERT(device_properties.apiVersion >= VK_API_VERSION_1_3);

            // Find a queue family that supports graphics and presentation
            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                                     nullptr);

            std::vector<VkQueueFamilyProperties> queue_family_properties(
                queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                                     queue_family_properties.data());

            for (uint32_t i = 0; i < queue_family_count; i++)
            {
                VkBool32 supports_present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface,
                                                     &supports_present);

                if (((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) !=
                     0U) &&
                    (supports_present != 0U))
                {
                    graphics_queue_index = i;
                    break;
                }
            }

            if (graphics_queue_index != UNIT32_MAX)
            {
                gpu = physical_device;
                break;
            }
        }

        mcs::MCS_ASSERT(graphics_queue_index != UNIT32_MAX);

        uint32_t device_extension_count; // NOLINT
        mcs::vulkan::utils::check_vk_result(vkEnumerateDeviceExtensionProperties(
            gpu, nullptr, &device_extension_count, nullptr));

        std::vector<VkExtensionProperties> device_extensions(device_extension_count);
        mcs::vulkan::utils::check_vk_result(vkEnumerateDeviceExtensionProperties(
            gpu, nullptr, &device_extension_count, device_extensions.data()));

        // Since this sample has visual output, the device needs to support the swapchain
        // extension
        std::vector<const char *> required_device_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        auto check_extensions = std::ranges::all_of(
            required_device_extensions, [&](const char *extension_name) {
                bool found = std::ranges::any_of(
                    device_extensions,
                    [&](const VkExtensionProperties &available_extension) {
                        return std::strcmp(available_extension.extensionName,
                                           extension_name) == 0;
                    });
                if (!found)
                {
                    std::println("Error: Required extension not found: {}",
                                 extension_name);
                }
                return found;
            });
        mcs::MCS_ASSERT(check_extensions);

        // Query for Vulkan 1.3 features
        VkPhysicalDeviceFeatures2 query_device_features2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan13Features query_vulkan13_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT
            query_extended_dynamic_state_features{
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT};
        query_device_features2.pNext = &query_vulkan13_features;
        query_vulkan13_features.pNext = &query_extended_dynamic_state_features;

        vkGetPhysicalDeviceFeatures2(gpu, &query_device_features2);
        // Check if Physical device supports Vulkan 1.3 features
        if (!query_vulkan13_features.dynamicRendering) // NOLINT
        {
            throw std::runtime_error("Dynamic Rendering feature is missing");
        }
        if (!query_vulkan13_features.synchronization2) // NOLINT
        {
            throw std::runtime_error("Synchronization2 feature is missing");
        }
        if (!query_extended_dynamic_state_features.extendedDynamicState) // NOLINT
        {
            throw std::runtime_error("Extended Dynamic State feature is missing");
        }

        {
            auto query = mcs::vulkan::structure_chain<
                VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{{}, {}, {}};
            vkGetPhysicalDeviceFeatures2(gpu, &query.head());

            if (query.get<1>().dynamicRendering == 0U)
            {
                throw std::runtime_error("Dynamic Rendering feature is missing");
            }
            if (query.get<1>().synchronization2 == 0U)
            {
                throw std::runtime_error("Synchronization2 feature is missing");
            }
            if (query.get<2>().extendedDynamicState == 0U)
            {
                throw std::runtime_error("Extended Dynamic State feature is missing");
            }
            mcs::MCS_ASSERT(
                query.get<VkPhysicalDeviceVulkan13Features>().dynamicRendering != 0U);
            mcs::MCS_ASSERT(
                query.get<VkPhysicalDeviceVulkan13Features>().synchronization2 != 0U);
            mcs::MCS_ASSERT(
                query.get<VkPhysicalDeviceVulkan13Features>().dynamicRendering != 0U);
            mcs::MCS_ASSERT(query.get<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>()
                                .extendedDynamicState != 0U);
        }

        {
            auto ps = mcs::vulkan::physical_device::availablePhysicalDevices(
                ctx.raw_instance());
            static_assert(std::is_same_v<decltype(ps), decltype(gpus)>);

            bool equal = std::ranges::equal(ps,  // 第一个范围
                                            gpus // 第二个范围
            );
            mcs::MCS_ASSERT(equal);
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
        window.teardown();
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }
}

constexpr static auto defaultPropertiesCheck() noexcept // NOLINT
{
    return [](const VkPhysicalDeviceProperties &device_properties) constexpr noexcept {
        return device_properties.apiVersion >= VK_API_VERSION_1_3;
    };
}
constexpr static auto defaultQueueFamilyCheck() noexcept // NOLINT
{
    return [](const VkQueueFamilyProperties &qfp) constexpr noexcept {
        return !!(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT);
    };
}
constexpr static auto defaultFeatures2Check() noexcept // NOLINT
{
    return [](const mcs::vulkan::physical_device &physicalDevice) constexpr noexcept
               -> bool {
        auto query =
            mcs::vulkan::structure_chain<VkPhysicalDeviceFeatures2,
                                         VkPhysicalDeviceVulkan13Features,
                                         VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{
                {}, {}, {}};
        physicalDevice.getFeatures2(query.head());
        auto &query_vulkan13_features =
            query.template get<VkPhysicalDeviceVulkan13Features>();
        auto &query_extended_dynamic_state_features =
            query.template get<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        return query_vulkan13_features.dynamicRendering &&
               query_vulkan13_features.synchronization2 &&
               query_extended_dynamic_state_features.extendedDynamicState;
    };
}

int main()
{
    // mcs::vulkan::wsi::glfw::Window
    try
    {
        using surface = mcs::vulkan::wsi::glfw::Window;

        surface window{};
        window.setup({.width = 800, .height = 600}, "test"); // NOLINT
        mcs::vulkan::context_base ctx;
        ctx.createInstance(
               mcs::vulkan::make_instance{}
                   .enableDebugExtension()
                   .enableSurfaceExtension<surface>()
                   .checkExtensionSupport()
                   .checkLayerSupport()
                   .build(mcs::vulkan::make_instance::defaultApplicationInfo()))
            .createSurface(window);

        using make_physical_device = mcs::vulkan::make_physical_device;
        using physical_device = mcs::vulkan::physical_device;

        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}; // NOLINTEND

        auto gpu = make_physical_device{ctx.raw_instance()}
                       .requiredDeviceProperties(::defaultPropertiesCheck())
                       .requiredQueueFamilyProperties(::defaultQueueFamilyCheck())
                       .requiredDeviceExtensions(requiredDeviceExtension)
                       .requiredFeatures(::defaultFeatures2Check())
                       .pickPhysicalDevice();
        mcs::MCS_ASSERT(gpu.valid());

        {
            auto gpu =
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
                        auto query = mcs::vulkan::structure_chain<
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
            mcs::MCS_ASSERT(gpu.valid());
        }

        {
            mcs::vulkan::context_base ctx;
            ctx.createInstance(
                   mcs::vulkan::make_instance{}
                       .enableDebugExtension()
                       .enableSurfaceExtension<surface>()
                       .checkExtensionSupport()
                       .checkLayerSupport()
                       .build({.sType = mcs::vulkan::sType<VkApplicationInfo>(),
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
                                                 &physicalDevice) constexpr noexcept
                                              -> bool {
                            auto query = mcs::vulkan::structure_chain<
                                VkPhysicalDeviceFeatures2,
                                VkPhysicalDeviceVulkan13Features,
                                VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{
                                {}, {}, {}};
                            physicalDevice.getFeatures2(query.head());
                            auto &query_vulkan13_features =
                                query.template get<VkPhysicalDeviceVulkan13Features>();
                            auto &query_extended_dynamic_state_features =
                                query.template get<
                                    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                            return query_vulkan13_features.dynamicRendering &&
                                   query_vulkan13_features.synchronization2 &&
                                   query_extended_dynamic_state_features
                                       .extendedDynamicState;
                        })
                        .pickPhysicalDevice());
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
        window.teardown();
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "main done\n";
    return 0;
}