#pragma once

#include "Instance.hpp"

namespace mcs::vulkan::core
{
    class PhysicalDevice
    {
        using value_type = VkPhysicalDevice;
        Instance *instance_{};
        value_type value_{};

      public:
        constexpr value_type *operator->() noexcept
        {
            return &value_;
        }
        constexpr const value_type *operator->() const noexcept
        {
            return &value_;
        }
        constexpr explicit operator bool() const noexcept
        {
            return value_ != nullptr;
        }
        constexpr value_type &operator*() noexcept
        {
            return value_;
        }
        constexpr const value_type &operator*() const noexcept
        {
            return value_;
        }
        PhysicalDevice() = default;
        constexpr PhysicalDevice(Instance &instance, VkPhysicalDevice value) noexcept
            : instance_{&instance}, value_{value}
        {
        }
        [[nodiscard]] auto enumeratePhysicalDevices() const
        {
            return instance_->enumeratePhysicalDevices();
        }
        [[nodiscard]] auto getProperties() const noexcept
        {
            return instance_->getPhysicalDeviceProperties(value_);
        }
        [[nodiscard]] auto getQueueFamilyProperties() const noexcept
        {
            return instance_->getPhysicalDeviceQueueFamilyProperties(value_);
        }
        [[nodiscard]] auto enumerateDeviceExtensionProperties() const
        {
            return instance_->enumerateDeviceExtensionProperties(value_);
        }
        constexpr auto getFeatures2(VkPhysicalDeviceFeatures2 *pFeatures) const noexcept
        {
            instance_->getPhysicalDeviceFeatures2(value_, pFeatures);
        }

        [[nodiscard]] auto getMemoryProperties() const noexcept
        {
            return instance_->getPhysicalDeviceMemoryProperties(value_);
        }

        constexpr bool getSurfaceSupportKHR(uint32_t queueFamilyIndex,
                                            VkSurfaceKHR surface) const
        {
            return instance_->getPhysicalDeviceSurfaceSupportKHR(value_, queueFamilyIndex,
                                                                 surface);
        }
        constexpr VkDevice createDevice(const VkDeviceCreateInfo *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator) const
        {
            return instance_->createDevice(value_, pCreateInfo, pAllocator);
        }
        constexpr auto getSurfaceCapabilitiesKHR(VkSurfaceKHR surface) const
        {
            return instance_->getPhysicalDeviceSurfaceCapabilitiesKHR(value_, surface);
        }
        constexpr auto getSurfaceFormatsKHR(VkSurfaceKHR surface) const
        {
            return instance_->getPhysicalDeviceSurfaceFormatsKHR(value_, surface);
        }
        constexpr auto getSurfacePresentModesKHR(VkSurfaceKHR surface) const
        {
            return instance_->getPhysicalDeviceSurfacePresentModesKHR(value_, surface);
        }
    };
}; // namespace mcs::vulkan::core