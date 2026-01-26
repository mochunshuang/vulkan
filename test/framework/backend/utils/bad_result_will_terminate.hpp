#pragma once

#include "mcs_terminate.hpp"
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan
{
    static constexpr auto bad_result_will_terminate(const VkResult &ret,
                                                    std::string_view msg) noexcept -> void
    {
        if (ret != VK_SUCCESS)
            mcs_terminate(msg);
    };

}; // namespace mcs::vulkan