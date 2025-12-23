#pragma once

#include "../utils/check_vk_result.hpp"
#include "../utils/mcslog.hpp"

#include <algorithm>
#include <vector>

namespace mcs::vulkan::vk_api
{
    struct vk_instance_api
    {
        [[nodiscard]] static constexpr auto availableExtension()
            -> std::vector<VkExtensionProperties>
        {
            // NOTE: Properties 只读的意思
            uint32_t instance_extension_count; // NOLINT
            utils::check_vk_result(::vkEnumerateInstanceExtensionProperties(
                nullptr, &instance_extension_count, nullptr));

            std::vector<VkExtensionProperties> available_instance_extensions(
                instance_extension_count);
            utils::check_vk_result(::vkEnumerateInstanceExtensionProperties(
                nullptr, &instance_extension_count,
                available_instance_extensions.data()));
            return available_instance_extensions;
        }
        [[nodiscard]] static bool checkExtensionSupport(
            const std::vector<const char *> &required,
            const std::vector<VkExtensionProperties> &available = availableExtension())
        {
            for (const auto *extension_name : required)
            {
                bool found = std::ranges::any_of(
                    available, [&](auto const &available_extension) noexcept {
                        return ::strcmp(available_extension.extensionName,
                                        extension_name) == 0;
                    });
                if (!found)
                {
                    // Output an error message for the missing extension
                    MCSLOG_ERROR("Required extension not found: {}", extension_name);
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static constexpr auto availableLayer()
            -> std::vector<VkLayerProperties>
        {
            uint32_t instance_layer_count; // NOLINT
            utils::check_vk_result(
                ::vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

            std::vector<VkLayerProperties> supported_instance_layers(
                instance_layer_count);
            utils::check_vk_result(::vkEnumerateInstanceLayerProperties(
                &instance_layer_count, supported_instance_layers.data()));
            return supported_instance_layers;
        }
        [[nodiscard]] static bool checkLayerSupport(
            const std::vector<const char *> &required,
            const std::vector<VkLayerProperties> &available = availableLayer())
        {
            for (const auto *layer_name : required)
            {
                if (bool found = std::ranges::any_of(available,
                                                     [&](auto const &lp) noexcept {
                                                         return ::strcmp(lp.layerName,
                                                                         layer_name) == 0;
                                                     });
                    not found)
                {
                    // Output an error message for the missing extension
                    MCSLOG_ERROR("Required layer not found: {}", layer_name);
                    return false;
                }
            }
            return true;
        }
    };
}; // namespace mcs::vulkan::vk_api