#pragma once

#include <vulkan/vulkan.h>

namespace mcs::vulkan::vk_api
{
    struct vk_logical_device_api
    {
        constexpr static auto createDevice(VkPhysicalDevice physicalDevice,
                                           const VkDeviceCreateInfo *pCreateInfo,
                                           const VkAllocationCallbacks *pAllocator,
                                           VkDevice *pDevice) noexcept
        {
            return ::vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
        }

        constexpr static VkQueue getDeviceQueue(VkDevice device,
                                                uint32_t queueFamilyIndex,
                                                uint32_t queueIndex) noexcept
        {
            VkQueue queue; // NOLINT
            ::vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
            return queue;
        }
    };

}; // namespace mcs::vulkan::vk_api