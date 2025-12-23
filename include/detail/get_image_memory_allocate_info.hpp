#pragma once

#include "logical_device.hpp"
#include "physical_device.hpp"
#include "sType.hpp"

namespace mcs::vulkan
{
    static constexpr VkMemoryAllocateInfo get_image_memory_allocate_info(
        const physical_device &physicalDevice, const logical_device &device,
        VkImage image, VkMemoryPropertyFlags properties)
    {
        VkMemoryRequirements memRequirements = device.getImageMemoryRequirements(image);
        return {.sType = sType<VkMemoryAllocateInfo>(),
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = physicalDevice.findMemoryType(
                    memRequirements.memoryTypeBits, properties)};
    }
}; // namespace mcs::vulkan