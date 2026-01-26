#pragma once

#include <format>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include <vector>

namespace mcs::vulkan::check
{
    constexpr static void check_layer_support(
        const std::vector<const char *> &required,
        const std::vector<VkLayerProperties> &available)
    {
        for (const auto *layer_name : required)
        {
            if (bool found = std::ranges::any_of(available,
                                                 [&](auto const &lp) noexcept {
                                                     return ::strcmp(lp.layerName,
                                                                     layer_name) == 0;
                                                 });
                not found)
                // Output an error message for the missing extension
                throw std::runtime_error{
                    std::format("Required layer not found: {}", layer_name)};
        }
    }
} // namespace mcs::vulkan::check