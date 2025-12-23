#pragma once

#include "sType.hpp"
#include "single_time_command_buffer.hpp"

namespace mcs::vulkan
{
    constexpr static void copy_buffer(const logical_device &device, VkQueue queue,
                                      VkCommandPool commandPool, VkBuffer srcBuffer,
                                      VkBuffer dstBuffer, VkDeviceSize size)
    {
        auto command_buffer = single_time_command_buffer{device, commandPool};

        command_buffer.begin();

        auto *commandBuffer = command_buffer.raw_data();

        VkBufferCopy copyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
        };
        ::vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        command_buffer.end();

        VkSubmitInfo submitInfo = {
            .sType = sType<VkSubmitInfo>(),
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
        };
        ::vkQueueSubmit(queue, 1, &submitInfo, nullptr);
        ::vkQueueWaitIdle(queue);
    }
}; // namespace mcs::vulkan