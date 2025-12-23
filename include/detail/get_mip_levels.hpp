#pragma once

#include <cmath>
#include <algorithm>

namespace mcs::vulkan
{
    constexpr static uint32_t get_mip_levels(uint32_t texWidth,
                                             uint32_t texHeight) noexcept
    {
        return static_cast<uint32_t>(
                   std::floor(std::log2(std::max(texWidth, texHeight)))) +
               1;
    }
}; // namespace mcs::vulkan