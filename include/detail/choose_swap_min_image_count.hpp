#pragma once

#include <vulkan/vulkan.h>

#include "./utils/mcs_assert.hpp"

namespace mcs::vulkan
{
    constexpr static uint32_t choose_swap_min_image_count(
        const VkSurfaceCapabilitiesKHR &capabilities) noexcept
    {
        MCS_ASSERT(capabilities.maxImageCount != 0);
        uint32_t minImageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount)
            minImageCount = capabilities.maxImageCount;
        return minImageCount;
    }
}; // namespace mcs::vulkan