#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace mcs::vulkan::vk_api
{
    struct vk_debug_api
    {
        static constexpr void addRequiredExtension(
            std::vector<const char *> &required_instance_extensions)
        {
            required_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        static constexpr void addRequiredLayer(
            std::vector<const char *> &requested_instance_layers)
        {
            constexpr auto VK_DEBUG_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
            requested_instance_layers.push_back(VK_DEBUG_LAYER_NAME);
        }
    };

}; // namespace mcs::vulkan::vk_api