#pragma once

#include "utils/mcs_assert.hpp"

#include <vulkan/vulkan.h>
#include <bit>
#include <concepts>

namespace mcs::vulkan
{
    template <typename Fun>
    auto *getFunPtr(VkInstance &instance) noexcept
    {
        if constexpr (std::same_as<Fun, PFN_vkDestroyDebugUtilsMessengerEXT>)
        {
            auto pfun = std::bit_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                ::vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            MCS_ASSERT(pfun != nullptr);
            return pfun;
        }
        else if constexpr (std::same_as<Fun, PFN_vkCreateDebugUtilsMessengerEXT>)
        {
            auto pfun = std::bit_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                ::vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            MCS_ASSERT(pfun != nullptr);
            return pfun;
        }
        else
        {
            static_assert(false, "not find.");
        }
    }

}; // namespace mcs::vulkan