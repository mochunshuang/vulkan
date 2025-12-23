#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace mcs::vulkan
{
    constexpr static VkPresentModeKHR choose_swap_present_mode(
        VkPresentModeKHR select,
        const std::vector<VkPresentModeKHR> &availablePresentModes) noexcept
    {
        for (const auto &availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == select)
                return availablePresentMode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}; // namespace mcs::vulkan