#pragma once

#include "begin_single_time_commands.hpp"
#include "end_single_time_commands.hpp"
#include <utility>

namespace mcs::vulkan
{
    constexpr static void copy_buffer_to_image(const logical_device &device,
                                               VkQueue graphicsQueue,
                                               VkCommandPool commandPool, VkImage image,
                                               VkBuffer buffer, uint32_t width,
                                               uint32_t height)
    {

        auto commandBuffer = begin_single_time_commands(device, commandPool);

        VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .mipLevel = 0,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1}};

        ::vkCmdCopyBufferToImage(commandBuffer.raw_data(), buffer, image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        end_single_time_commands(graphicsQueue, std::move(commandBuffer));
    }
}; // namespace mcs::vulkan