#pragma once

#include <cstdint>
#include <type_traits>
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan::core
{

    template <typename T>
    consteval VkIndexType get_index_type()
    {
        if (std::is_same_v<T, uint16_t>)
            return VK_INDEX_TYPE_UINT16;
        if (std::is_same_v<T, uint32_t>)
            return VK_INDEX_TYPE_UINT32;
        if (std::is_same_v<T, uint8_t>)
            return VK_INDEX_TYPE_UINT8;
        throw;
    }
}; // namespace mcs::vulkan::core