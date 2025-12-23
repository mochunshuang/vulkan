#pragma once

#include "./vk_api/vk_physical_device_api.hpp"
#include "./vk_api/vk_logical_device_api.hpp"
#include "utils/check_vk_result.hpp"

namespace mcs::vulkan
{
    struct physical_device : vk_api::vk_physical_device_api
    {
        using value_type = VkPhysicalDevice;

        constexpr physical_device() = default;
        constexpr explicit physical_device(const value_type &physical_device) noexcept
            : physicalDevice_{physical_device}
        {
        }

        [[nodiscard]] constexpr auto raw_data() const noexcept // NOLINT
        {
            return physicalDevice_;
        }
        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return physicalDevice_ != nullptr;
        }

        constexpr static std::vector<value_type> availablePhysicalDevices(
            VkInstance instance)
        {
            return enumeratePhysicalDevices(instance);
        }

        [[nodiscard]] constexpr VkPhysicalDeviceProperties getProperties() const noexcept
        {
            return getPhysicalDeviceProperties(physicalDevice_);
        }

        [[nodiscard]] constexpr auto getQueueFamilyProperties() const
            -> std::vector<VkQueueFamilyProperties>
        {
            return getPhysicalDeviceQueueFamilyProperties(physicalDevice_);
        }

        [[nodiscard]] constexpr auto availableDeviceExtensionProperties() const
            -> std::vector<VkExtensionProperties>
        {
            return enumerateDeviceExtensionProperties(physicalDevice_);
        }
        constexpr void getFeatures2(VkPhysicalDeviceFeatures2 &features) const noexcept
        {
            getPhysicalDeviceFeatures2(physicalDevice_, &features);
        }

        constexpr bool getSurfaceSupportKHR(uint32_t queueFamilyIndex,
                                            VkSurfaceKHR surface) const noexcept
        {
            return getPhysicalDeviceSurfaceSupportKHR(physicalDevice_, queueFamilyIndex,
                                                      surface);
        }

        constexpr VkDevice createDevice(const VkDeviceCreateInfo *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator)
        {
            VkDevice device; // NOLINT
            utils::check_vk_result(vk_api::vk_logical_device_api::createDevice(
                physicalDevice_, pCreateInfo, pAllocator, &device));
            return device;
        }

        [[nodiscard]] VkSampleCountFlagBits getMaxUsableSampleCount() const noexcept
        {
            VkPhysicalDeviceProperties physicalDeviceProperties = getProperties();
            auto counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                          physicalDeviceProperties.limits.framebufferDepthSampleCounts;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_64_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_64_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_32_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_32_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_16_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_16_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT;
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        }
        auto getSurfaceCapabilitiesKHR(VkSurfaceKHR surface) const noexcept
        {
            return getPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface);
        }
        auto getSurfaceFormatsKHR(VkSurfaceKHR surface) const
        {
            return getPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface);
        }
        auto getSurfacePresentModesKHR(VkSurfaceKHR surface) const
        {
            return getPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface);
        }

        [[nodiscard]] VkPhysicalDeviceMemoryProperties getMemoryProperties()
            const noexcept
        {
            VkPhysicalDeviceMemoryProperties memProperties;
            ::vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
            return memProperties;
        }
        [[nodiscard]] constexpr uint32_t findMemoryType(

            uint32_t typeFilter, VkMemoryPropertyFlags properties) const
        {
            VkPhysicalDeviceMemoryProperties memProperties = getMemoryProperties();
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
            {
                auto &memoryTypes = memProperties.memoryTypes[i]; // NOLINT
                if (((typeFilter & (1 << i)) != 0U) &&
                    (memoryTypes.propertyFlags & properties) == properties)
                {
                    return i;
                }
            }
            throw std::runtime_error("failed to find suitable memory type!");
        }

        [[nodiscard]] auto getFormatProperties(VkFormat format) const noexcept
        {
            return getPhysicalDeviceFormatProperties(physicalDevice_, format);
        }

      private:
        value_type physicalDevice_ = VK_NULL_HANDLE;
    };

}; // namespace mcs::vulkan