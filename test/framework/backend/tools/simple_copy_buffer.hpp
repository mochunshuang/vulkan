#pragma once

#include "../CommandPool.hpp"
#include "../Queue.hpp"
#include "../Fence.hpp"
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan::tools
{
    static constexpr auto simple_copy_buffer(const core::CommandPool &commandpool,
                                             const core::Queue &queue, VkBuffer srcBuffer,
                                             VkBuffer dstBuffer,
                                             const std::vector<VkBufferCopy> &regions)
    {
        const auto *logicalDevice = commandpool.logicalDevice();
        auto commandCopyBuffer = commandpool.allocateOneCommandBuffer(
            {.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1});

        commandCopyBuffer.begin({.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});
        commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, regions);
        commandCopyBuffer.end();

        core::Fence fence{*logicalDevice,
                          {.sType = core::sType<VkFenceCreateInfo>(), .flags = 0}};
        queue.submit(1,
                     {.sType = core::sType<VkSubmitInfo>(),
                      .commandBufferCount = 1,
                      .pCommandBuffers = &*commandCopyBuffer},
                     *fence);
        check_vkresult(logicalDevice->waitForFences(1, *fence, VK_TRUE, UINT64_MAX));
    }

}; // namespace mcs::vulkan::tools