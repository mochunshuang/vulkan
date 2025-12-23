#pragma once

#include "get_image_memory_allocate_info.hpp"

namespace mcs::vulkan
{
    static constexpr auto create_image(const physical_device &physicalDevice,
                                       const logical_device &device,
                                       const VkImageCreateInfo &imageCreateInfo,
                                       VkMemoryPropertyFlags properties)
    {
        VkImage image = nullptr;
        VkDeviceMemory imageMemory = nullptr;
        try
        {
            image = device.createImage(imageCreateInfo);
            VkMemoryAllocateInfo allocInfo =
                get_image_memory_allocate_info(physicalDevice, device, image, properties);
            imageMemory = device.allocateMemory(allocInfo, nullptr);
            device.bindImageMemory(image, imageMemory, 0);
            return std::make_pair(image, imageMemory);
        }
        catch (...)
        {
            if (image != nullptr)
                device.destroyImage(image, nullptr);
            if (imageMemory != nullptr)
                device.freeMemory(imageMemory, nullptr);
            throw;
        }
    }
}; // namespace mcs::vulkan