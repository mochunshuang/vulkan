#pragma once

#include "simple_copy_buffer.hpp"
#include "staging_buffer.hpp"

namespace mcs::vulkan::tools
{
    [[nodiscard]] static constexpr auto simple_index_buffer(
        const core::CommandPool &commandpool, const core::Queue &queue, const void *src,
        size_t buffer_size, VkMemoryMapFlags flags = 0)
    {
        const auto &logicalDevice = *commandpool.logicalDevice();
        auto indexBuffer = create_buffer(
            logicalDevice,
            {.sType = core::sType<VkBufferCreateInfo>(),
             .size = buffer_size,
             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // NOLINTNEXTLINE
        const auto stagingBuffer = staging_buffer(logicalDevice, buffer_size);
        stagingBuffer.copyDataToBuffer(src, buffer_size, flags);
        simple_copy_buffer(commandpool, queue, stagingBuffer.buffer(),
                           indexBuffer.buffer(), {VkBufferCopy{.size = buffer_size}});
        return indexBuffer;
    }

}; // namespace mcs::vulkan::tools