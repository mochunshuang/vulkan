#pragma once

#include "instance_base.hpp"
#include "utils/mcs_assert.hpp"
#include "utils/check_vkresult.hpp"
#include "utils/mcs_terminate.hpp"
#include <cstddef>
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan::core
{
    class Instance : instance_base
    {
      public:
        using instance_base::instance_base;
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
        constexpr void destroyInstance(
            const VkAllocationCallbacks *pAllocator) const noexcept
        {
            ::vkDestroyInstance(value_, pAllocator);
        }

        constexpr auto createDebugUtilsMessengerEXT(
            const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
            const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createDebugUtilsMessengerEXT_ != nullptr);
            VkDebugUtilsMessengerEXT messenger; // NOLINT
            check_vkresult(createDebugUtilsMessengerEXT_(value_, pCreateInfo, pAllocator,
                                                         &messenger),
                           "createDebugUtilsMessengerEXT error");
            return messenger;
        }
        constexpr void destroyDebugUtilsMessengerEXT(
            VkDebugUtilsMessengerEXT debug_,
            const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroyDebugUtilsMessengerEXT_ != nullptr);
            destroyDebugUtilsMessengerEXT_(value_, debug_, pAllocator);
        }
        constexpr void destroySurfaceKHR(VkSurfaceKHR surface,
                                         const VkAllocationCallbacks *pAllocator) noexcept
        {
            MCS_ASSERT(destroySurfaceKHR_ != nullptr);
            destroySurfaceKHR_(value_, surface, pAllocator);
        }

        [[nodiscard]] constexpr auto enumeratePhysicalDevices() const
        {
            MCS_ASSERT(enumeratePhysicalDevices_ != nullptr);
            uint32_t gpuCount; // NOLINT
            enumeratePhysicalDevices_(value_, &gpuCount, nullptr);
            if (gpuCount == 0)
                mcs_terminate("No device with Vulkan support found");

            std::vector<VkPhysicalDevice> physicalDevices{gpuCount};
            check_vkresult(
                enumeratePhysicalDevices_(value_, &gpuCount, physicalDevices.data()),
                "enumeratePhysicalDevices error");
            return physicalDevices;
        }

        constexpr auto getPhysicalDeviceProperties(
            VkPhysicalDevice physicalDevice) const noexcept
        {
            MCS_ASSERT(getPhysicalDeviceProperties_ != nullptr);
            VkPhysicalDeviceProperties deviceProperties;
            getPhysicalDeviceProperties_(physicalDevice, &deviceProperties);
            return deviceProperties;
        }
        constexpr auto getPhysicalDeviceQueueFamilyProperties(
            VkPhysicalDevice physicalDevice) const
        {
            MCS_ASSERT(getPhysicalDeviceQueueFamilyProperties_ != nullptr);
            uint32_t queueFamilyCount = 0;
            getPhysicalDeviceQueueFamilyProperties_(physicalDevice, &queueFamilyCount,
                                                    nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            getPhysicalDeviceQueueFamilyProperties_(physicalDevice, &queueFamilyCount,
                                                    queueFamilies.data());
            return queueFamilies;
        }

        [[nodiscard]] auto enumerateDeviceExtensionProperties(
            VkPhysicalDevice physicalDevice) const
        {
            MCS_ASSERT(enumerateDeviceExtensionProperties_ != nullptr);
            uint32_t extensionCount; // NOLINT
            check_vkresult(enumerateDeviceExtensionProperties_(physicalDevice, nullptr,
                                                               &extensionCount, nullptr));
            std::vector<VkExtensionProperties> availableDeviceExtensions{extensionCount};
            check_vkresult(enumerateDeviceExtensionProperties_(
                physicalDevice, nullptr, &extensionCount,
                availableDeviceExtensions.data()));
            return availableDeviceExtensions;
        }
        [[nodiscard]] auto getPhysicalDeviceFeatures2(
            VkPhysicalDevice physicalDevice,
            VkPhysicalDeviceFeatures2 *pFeatures) const noexcept
        {
            MCS_ASSERT(getPhysicalDeviceFeatures2_ != nullptr);
            getPhysicalDeviceFeatures2_(physicalDevice, pFeatures);
        }

        [[nodiscard]] auto getPhysicalDeviceMemoryProperties(
            VkPhysicalDevice physicalDevice) const noexcept
        {
            MCS_ASSERT(getPhysicalDeviceMemoryProperties_ != nullptr);
            VkPhysicalDeviceMemoryProperties prop;
            getPhysicalDeviceMemoryProperties_(physicalDevice, &prop);
            return prop;
        }
        [[nodiscard]] auto getPhysicalDeviceSurfaceSupportKHR(
            VkPhysicalDevice physicalDevice, size_t queueFamilyIndex,
            VkSurfaceKHR surface) const
        {
            MCS_ASSERT(getPhysicalDeviceSurfaceSupportKHR_ != nullptr);
            VkBool32 support; // NOLINT
            check_vkresult(getPhysicalDeviceSurfaceSupportKHR_(
                physicalDevice, queueFamilyIndex, surface, &support));
            return support == VK_TRUE;
        }
        constexpr auto createDevice(VkPhysicalDevice physicalDevice,
                                    const VkDeviceCreateInfo *pCreateInfo,
                                    const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createDevice_ != nullptr);
            VkDevice device; // NOLINT
            check_vkresult(
                createDevice_(physicalDevice, pCreateInfo, pAllocator, &device));
            return device;
        }

        constexpr auto getPhysicalDeviceSurfaceCapabilitiesKHR(
            VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const
        {
            MCS_ASSERT(getPhysicalDeviceSurfaceCapabilitiesKHR_ != nullptr);
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            check_vkresult(getPhysicalDeviceSurfaceCapabilitiesKHR_(
                physicalDevice, surface, &surfaceCapabilities));
            return surfaceCapabilities;
        }
        constexpr auto getPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                                          VkSurfaceKHR surface) const
        {
            MCS_ASSERT(getPhysicalDeviceSurfaceFormatsKHR_ != nullptr);
            uint32_t formatCount; // NOLINT
            check_vkresult(getPhysicalDeviceSurfaceFormatsKHR_(physicalDevice, surface,
                                                               &formatCount, nullptr));
            std::vector<VkSurfaceFormatKHR> formats(formatCount);
            check_vkresult(getPhysicalDeviceSurfaceFormatsKHR_(
                physicalDevice, surface, &formatCount, formats.data()));
            return formats;
        }

        constexpr auto getPhysicalDeviceSurfacePresentModesKHR(
            VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const
        {
            MCS_ASSERT(getPhysicalDeviceSurfacePresentModesKHR_ != nullptr);
            uint32_t presentModeCount; // NOLINT
            check_vkresult(getPhysicalDeviceSurfacePresentModesKHR_(
                physicalDevice, surface, &presentModeCount, nullptr));
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            check_vkresult(getPhysicalDeviceSurfacePresentModesKHR_(
                physicalDevice, surface, &presentModeCount, presentModes.data()));
            return presentModes;
        }
    };

}; // namespace mcs::vulkan::core