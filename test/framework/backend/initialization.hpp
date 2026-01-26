#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

#include "utils/bad_result_will_terminate.hpp"

#include "app_type.hpp"

namespace mcs::vulkan::core
{
    // https://vulkan.lunarg.com/doc/view/latest/windows/antora/spec/latest/chapters/initialization.html
    // https://docs.vulkan.net.cn/spec/latest/chapters/initialization.html
    // 原因: 在使用 Vulkan 之前，应用程序必须通过加载 Vulkan 命令并创建一个 VkInstance
    // 对象来初始化它。

    // NOLINTBEGIN
    static constexpr PFN_vkVoidFunction getFuncPtr(VkInstance instance,
                                                   const char *pName) noexcept
    {
        return ::vkGetInstanceProcAddr(instance, pName);
    }

    // 好处: 可以避免 VkDevice 对象的内部调度的开销
    static constexpr PFN_vkVoidFunction getFuncPtr(VkDevice device,
                                                   const char *pName) noexcept
    {
        return ::vkGetDeviceProcAddr(device, pName);
    }

    // 全局函数
    static constexpr app_version instanceVersion() noexcept
    {
        uint32_t apiVersion = 0;
        bad_result_will_terminate(vkEnumerateInstanceVersion(&apiVersion),
                                  "Failed to query Vulkan version");
        return app_version{.major = VK_VERSION_MAJOR(apiVersion),
                           .minor = VK_VERSION_MINOR(apiVersion),
                           .patch = VK_VERSION_PATCH(apiVersion)};
    }

    static constexpr std::vector<VkLayerProperties> instanceLayerProperties(
        VkLayerProperties *pProperties = nullptr)
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, pProperties);
        std::vector<VkLayerProperties> availableLayers{layerCount};
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        return availableLayers;
    }

    static constexpr std::vector<VkExtensionProperties> instanceExtensionPropertie(
        const char *pLayerName = nullptr)
    {
        // 枚举全局扩展（不属于任何层）
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(pLayerName, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableinstanceExtensions{extensionCount};
        vkEnumerateInstanceExtensionProperties(pLayerName, &extensionCount,
                                               availableinstanceExtensions.data());
        return availableinstanceExtensions;
    }

    // NOLINTEND

}; // namespace mcs::vulkan::core

namespace std
{
    template <>
    struct formatter<VkLayerProperties>
    {
        static constexpr auto parse(std::format_parse_context &ctx) noexcept
        {
            return ctx.begin();
        }
        static constexpr auto format(const VkLayerProperties &layer,
                                     std::format_context &ctx)
        {
            std::string_view name{layer.layerName};

            uint32_t spec_major = VK_VERSION_MAJOR(layer.specVersion);
            uint32_t spec_minor = VK_VERSION_MINOR(layer.specVersion);
            uint32_t spec_patch = VK_VERSION_PATCH(layer.specVersion);

            uint32_t impl_major = VK_VERSION_MAJOR(layer.implementationVersion);
            uint32_t impl_minor = VK_VERSION_MINOR(layer.implementationVersion);
            uint32_t impl_patch = VK_VERSION_PATCH(layer.implementationVersion);

            std::string_view desc{layer.description};
            return std::format_to(ctx.out(),
                                  "layerName: {}, specVersion: {}.{}.{}, "
                                  "implementationVersion: {}.{}.{}, description: {}",
                                  name, spec_major, spec_minor, spec_patch, impl_major,
                                  impl_minor, impl_patch, desc);
        }
    };

    template <>
    struct formatter<VkExtensionProperties>
    {
        static constexpr auto parse(std::format_parse_context &ctx) noexcept
        {
            return ctx.begin();
        }
        static constexpr auto format(const VkExtensionProperties &ext,
                                     std::format_context &ctx)
        {
            std::string_view name{ext.extensionName};

            uint32_t spec_major = VK_VERSION_MAJOR(ext.specVersion);
            uint32_t spec_minor = VK_VERSION_MINOR(ext.specVersion);
            uint32_t spec_patch = VK_VERSION_PATCH(ext.specVersion);

            return std::format_to(ctx.out(), "extensionName: {}, specVersion: {}.{}.{}",
                                  name, spec_major, spec_minor, spec_patch);
        }
    };

}; // namespace std