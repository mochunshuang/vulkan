#pragma once

#include <stdexcept>

#include <vulkan/vulkan.h>

namespace mcs::vulkan::utils
{
    static constexpr void check_vk_result(const VkResult &err)
    {
        if (err != VkResult::VK_SUCCESS)
            throw std::runtime_error("Detected Vulkan error: ");
    }

}; // namespace mcs::vulkan::utils