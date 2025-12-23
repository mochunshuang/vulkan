#pragma once

#include "command_buffer.hpp"
#include "sType.hpp"
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan
{
    struct single_time_command_buffer final : command_buffer
    {
        constexpr single_time_command_buffer(const logical_device &device,
                                             VkCommandPool commandPool)
            : command_buffer{
                  device,
                  VkCommandBufferAllocateInfo{
                      .sType = sType<VkCommandBufferAllocateInfo>(),
                      .commandPool = commandPool,
                      .level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                      .commandBufferCount = 1}}
        {
        }
        constexpr void begin() const
        {
            VkCommandBufferBeginInfo beginInfo{
                .sType = sType<VkCommandBufferBeginInfo>(),
                .flags = VkCommandBufferUsageFlagBits::
                    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
            if (::vkBeginCommandBuffer(raw_data(), &beginInfo) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to begin command buffer!");
        }
        constexpr void end() const
        {
            if (::vkEndCommandBuffer(raw_data()) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to end command buffer!");
        }
    };

}; // namespace mcs::vulkan