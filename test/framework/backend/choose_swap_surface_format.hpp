#pragma once

#include "utils/make_vk_exception.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace mcs::vulkan
{
    constexpr static VkSurfaceFormatKHR choose_swap_surface_format(
        const VkSurfaceFormatKHR &select,
        const std::vector<VkSurfaceFormatKHR> &availableFormats)
    {
        for (const auto &availableFormat : availableFormats)
        {
            if (availableFormat.format == select.format &&
                availableFormat.colorSpace == select.colorSpace)
                return availableFormat;
        }
        throw make_vk_exception("choose_swap_surface_format failure.");
    }
}; // namespace mcs::vulkan
