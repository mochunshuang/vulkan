#pragma once

#include <vulkan/vulkan_core.h>

namespace mcs::vulkan
{
    static constexpr bool bad_result(const VkResult &ret) noexcept
    {
        return ret != VK_SUCCESS;
    }
}; // namespace mcs::vulkan