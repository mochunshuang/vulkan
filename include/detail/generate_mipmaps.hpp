#pragma once

#include "logical_device.hpp"
#include "physical_device.hpp"
#include "begin_single_time_commands.hpp"
#include "end_single_time_commands.hpp"
#include "utils/vk_exception.hpp"
#include <utility>

namespace mcs::vulkan
{
    struct mipmaps_param
    {
        int32_t width;
        int32_t height;
        uint32_t mip_levels;
    };

    constexpr static void generate_mipmaps(const physical_device &physicalDevice,
                                           const logical_device &device,
                                           VkCommandPool commandPool, VkQueue queue,
                                           VkImage image, VkFormat imageFormat,
                                           mipmaps_param param)
    {
        auto [texWidth, texHeight, mipLevels] = param;
        VkFormatProperties formatProperties =
            physicalDevice.getFormatProperties(imageFormat);

        if ((formatProperties.optimalTilingFeatures &
             VkFormatFeatureFlagBits::
                 VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0U)
            throw utils::make_vk_exception(
                "texture image format does not support linear blitting!");

        auto command = begin_single_time_commands(device, commandPool);
        auto *commandBuffer = command.raw_data();

        VkImageMemoryBarrier barrier = {
            .sType = sType<VkImageMemoryBarrier>(),
            .srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {.aspectMask =
                                     VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        // 我们将进行几个转换，所以我们将重用这个VkImageMemoryBarrier。上面设置的字段对于所有屏障将保持不变
        for (uint32_t i = 1; i < mipLevels; i++)
        {
            // 将第i-1级从TRANSFER_DST_OPTIMAL转换为TRANSFER_SRC_OPTIMAL
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;

            ::vkCmdPipelineBarrier(
                commandBuffer, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                nullptr, 1, &barrier);

            // 设置blit操作的参数
            VkImageBlit blit = {};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;

            blit.srcOffsets[0] = {.x = 0, .y = 0, .z = 0};
            blit.srcOffsets[1] = {.x = mipWidth, .y = mipHeight, .z = 1};

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            blit.dstOffsets[0] = {.x = 0, .y = 0, .z = 0};
            blit.dstOffsets[1] = {.x = mipWidth > 1 ? mipWidth / 2 : 1,
                                  .y = mipHeight > 1 ? mipHeight / 2 : 1,
                                  .z = 1};

            // NOTE: 该命令执行复制、缩放和过滤操作。
            // 我们的纹理图像现在有多个mip级别，但是暂存缓冲区只能用于填充mip级别0
            // 如果您使用专用传输队列（如顶点缓冲区中建议的），请注意：vkCmdBlitImage必须提交到具有图形功能的队列
            ::vkCmdBlitImage(commandBuffer, image,
                             VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                             VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &blit, VK_FILTER_LINEAR);

            // 将第i-1级从TRANSFER_SRC_OPTIMAL转换为SHADER_READ_ONLY_OPTIMAL
            barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;

            ::vkCmdPipelineBarrier(
                commandBuffer, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                nullptr, 0, nullptr, 1, &barrier);

            // 我们将当前mip维度除以2。我们在除法之前检查每个维度，以确保维度永远不会变成0
            if (mipWidth > 1)
                mipWidth /= 2;
            if (mipHeight > 1)
                mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;

        ::vkCmdPipelineBarrier(
            commandBuffer, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
            VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
            0, nullptr, 1, &barrier);

        end_single_time_commands(queue, std::move(command));

        // NOTE: 应该注意的是，在运行时生成mipmap级别在实践中并不常见。
        //  通常它们是预先生成的，并与基本级别一起存储在纹理文件中，以提高加载速度

        // NOTE:总之，每个mip级别都要像加载原始图像一样加载到图像中。
    }
}; // namespace mcs::vulkan