#pragma once

#include <vulkan/vulkan.h>

namespace mcs::vulkan::vk_api
{
    struct vk_surface_api
    {
        static constexpr void destroy(VkInstance instance,
                                      VkSurfaceKHR &surface_) noexcept
        {
            ::vkDestroySurfaceKHR(instance, surface_, nullptr);
        }
    };

}; // namespace mcs::vulkan::vk_api