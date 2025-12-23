#pragma once

#include <algorithm>
#include <limits>
#include <vulkan/vulkan.h>

namespace mcs::vulkan
{
    template <typename window_type>
    constexpr static VkExtent2D choose_swap_extent(
        const window_type &window, const VkSurfaceCapabilitiesKHR &capabilities) noexcept
    {
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
            return capabilities.currentExtent;

        auto [width, height] = window.getFramebufferSize();
        return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width),
                std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height)};
    }
}; // namespace mcs::vulkan