#pragma once

#include "sType.hpp"
#include "single_time_command_buffer.hpp"

namespace mcs::vulkan
{
    static constexpr void end_single_time_commands(VkQueue queue,
                                                   single_time_command_buffer command)
    {
        command.end();

        VkCommandBuffer commandBuffer = command.raw_data();
        VkSubmitInfo submitInfo{.sType = sType<VkSubmitInfo>(),
                                .commandBufferCount = 1,
                                .pCommandBuffers = &commandBuffer};
        if (vkQueueSubmit(queue, 1, &submitInfo, nullptr) != VK_SUCCESS)
            throw utils::make_vk_exception("failed to queue submit!");

        if (vkQueueWaitIdle(queue) != VK_SUCCESS)
            throw utils::make_vk_exception("failed to queue wait idle!");
    }
}; // namespace mcs::vulkan