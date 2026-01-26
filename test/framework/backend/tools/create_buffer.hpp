#pragma once

#include "../LogicalDevice.hpp"
#include "../buffer_base.hpp"
#include "../structure_chain.hpp"

namespace mcs::vulkan::tools
{
    [[nodiscard]] static constexpr core::buffer_base create_buffer(
        const core::LogicalDevice &device, const VkBufferCreateInfo &bufferInfo,
        VkMemoryPropertyFlags properties)
    {
        VkBuffer buffer = nullptr;
        VkDeviceMemory bufferMemory = nullptr;

        try
        {
            buffer = device.createBuffer(bufferInfo, device.allocator());
            VkMemoryRequirements memRequirements =
                device.getBufferMemoryRequirements(buffer);

            VkMemoryAllocateInfo allocInfo{
                .sType = core::sType<VkMemoryAllocateInfo>(),
                .allocationSize = memRequirements.size,
                .memoryTypeIndex =
                    device.findMemoryType(memRequirements.memoryTypeBits, properties)};

            bufferMemory = device.allocateMemory(allocInfo, device.allocator());
            device.bindBufferMemory(buffer, bufferMemory, 0);
            return core::buffer_base{device, buffer, bufferMemory};
        }
        catch (...)
        {
            if (buffer != nullptr)
                device.destroyBuffer(buffer, device.allocator());
            if (bufferMemory != nullptr)
                device.freeMemory(bufferMemory, device.allocator());
            throw;
        }
    }

    template <typename... BN, typename... MN>
    [[nodiscard]] static constexpr core::buffer_base create_buffer(
        const core::LogicalDevice &device,
        core::structure_chain<VkBufferCreateInfo, BN...> bufferInfo,
        core::structure_chain<VkMemoryAllocateInfo, MN...> allocInfo,
        VkMemoryPropertyFlagBits properties)
    {
        VkBuffer buffer = nullptr;
        VkDeviceMemory bufferMemory = nullptr;
        try
        {
            buffer = device.createBuffer(bufferInfo.head(), device.allocator());
            VkMemoryRequirements memRequirements =
                device.getBufferMemoryRequirements(buffer);

            allocInfo.head().allocationSize = memRequirements.size;
            allocInfo.head().memoryTypeIndex =
                device.findMemoryType(memRequirements.memoryTypeBits, properties);

            bufferMemory = device.allocateMemory(allocInfo.head(), device.allocator());
            device.bindBufferMemory(buffer, bufferMemory, 0);
            return core::buffer_base{device, buffer, bufferMemory};
        }
        catch (...)
        {
            if (buffer != nullptr)
                device.destroyBuffer(buffer, device.allocator());
            if (bufferMemory != nullptr)
                device.freeMemory(bufferMemory, device.allocator());
            throw;
        }
    }

}; // namespace mcs::vulkan::tools