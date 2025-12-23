#pragma once

#include "logical_device.hpp"
#include "physical_device.hpp"
#include "sType.hpp"

namespace mcs::vulkan
{
    static constexpr VkMemoryAllocateInfo get_buffer_memory_allocate_info(
        const physical_device &physicalDevice, const logical_device &device,
        VkBuffer buffer, VkMemoryPropertyFlags properties)
    {
        VkMemoryRequirements memRequirements = device.getBufferMemoryRequirements(buffer);
        return {.sType = sType<VkMemoryAllocateInfo>(),
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = physicalDevice.findMemoryType(
                    memRequirements.memoryTypeBits, properties)};
    }
}; // namespace mcs::vulkan
