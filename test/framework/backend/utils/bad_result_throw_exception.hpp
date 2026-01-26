#pragma once

#include <vulkan/vulkan_core.h>

#include <string>
#include <stdexcept>

namespace mcs::vulkan
{
    static constexpr auto bad_result_throw_exception(const VkResult &ret,
                                                     const std::string &msg) -> void
    {
        if (ret != VK_SUCCESS)
            throw std::runtime_error{msg};
    };

}; // namespace mcs::vulkan
