#pragma once

#include "PhysicalDevice.hpp"
#include "logical_device_base.hpp"

namespace mcs::vulkan::core
{
    class LogicalDevice : public logical_device_base
    {
        VkAllocationCallbacks *allocator_{};
        PhysicalDevice *physicalDevice_{};

      public:
        using logical_device_base::logical_device_base;
        constexpr LogicalDevice(VkDevice value, VkAllocationCallbacks *allocator,
                                PhysicalDevice *physicalDevice) noexcept
            : logical_device_base{value}, allocator_{allocator},
              physicalDevice_{physicalDevice}
        {
        }

        [[nodiscard]] constexpr VkAllocationCallbacks *allocator() const noexcept
        {
            return allocator_;
        }
        [[nodiscard]] constexpr auto physicalDevice() const noexcept
        {
            return physicalDevice_;
        }

        // API:
        [[nodiscard]] auto findMemoryType(uint32_t typeFilter,
                                          VkMemoryPropertyFlags properties) const
        {
            VkPhysicalDeviceMemoryProperties memProperties =
                physicalDevice_->getMemoryProperties();

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
            {
                if (((typeFilter & (1 << i)) != 0U) && // NOLINTNEXTLINE
                    ((memProperties.memoryTypes[i].propertyFlags) & properties) ==
                        properties)
                    return i;
            }
            throw std::runtime_error("failed to find suitable memory type!");
        }
    };
}; // namespace mcs::vulkan::core