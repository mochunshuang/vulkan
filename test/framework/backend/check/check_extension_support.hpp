#pragma once

#include <format>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include <vector>

namespace mcs::vulkan::check
{

    static constexpr auto check_extension_support(
        const std::vector<const char *> &required,
        const std::vector<VkExtensionProperties> &available)
    {
        for (const auto *extension_name : required)
        {
            bool found = std::ranges::any_of(
                available, [&](auto const &available_extension) noexcept {
                    return ::strcmp(available_extension.extensionName, extension_name) ==
                           0;
                });
            if (!found)
                throw std::runtime_error{
                    std::format("Required extension not found: {}", extension_name)};
        }
    }
} // namespace mcs::vulkan::check