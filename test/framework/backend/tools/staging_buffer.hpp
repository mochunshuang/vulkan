#pragma once

#include "create_buffer.hpp"
#include <cstddef>

namespace mcs::vulkan::tools
{
    static constexpr auto staging_buffer(const core::LogicalDevice &device,
                                         size_t buffer_size)
    {
        return create_buffer(device,
                             {.sType = core::sType<VkBufferCreateInfo>(),
                              .size = buffer_size,
                              .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

}; // namespace mcs::vulkan::tools