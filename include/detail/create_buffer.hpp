#pragma once

#include "buffer_base.hpp"
#include "get_buffer_memory_allocate_info.hpp"

namespace mcs::vulkan
{
    static constexpr buffer_base create_buffer(const physical_device &physicalDevice,
                                               const logical_device &device,
                                               const VkBufferCreateInfo &createInfo,
                                               VkMemoryPropertyFlags properties)
    {

        VkBuffer buffer = nullptr;
        VkDeviceMemory bufferMemory = nullptr;
        try
        {
            buffer = device.createBuffer(createInfo, nullptr);

            VkMemoryAllocateInfo allocInfo = get_buffer_memory_allocate_info(
                physicalDevice, device, buffer, properties);
            bufferMemory = device.allocateMemory(allocInfo, nullptr);
            device.bindBufferMemory(buffer, bufferMemory, 0);
            return buffer_base{device, buffer, bufferMemory};
        }
        catch (...)
        {
            if (buffer != nullptr)
                device.destroyBuffer(buffer);
            if (bufferMemory != nullptr)
                device.freeMemory(bufferMemory, nullptr);
            throw;
        }
    }
}; // namespace mcs::vulkan