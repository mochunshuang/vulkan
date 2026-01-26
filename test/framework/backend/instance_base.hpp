#pragma once

#include "proc_addr.hpp"

#include "utils/mcs_assert.hpp"
#include "utils/mcslog.hpp"

namespace mcs::vulkan::core
{
    struct instance_base
    {
      private:
        template <typename Fn>
        constexpr auto funPtr(const char *pName) noexcept
        {
            return proc_addr<Fn>(value_, pName);
        }

      public:
        using value_type = VkInstance;

        // NOLINTBEGIN
        value_type value_{};
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT_{};
        PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT_{};
        PFN_vkDestroySurfaceKHR destroySurfaceKHR_{};
        PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices_{};
        PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties_{};
        PFN_vkGetPhysicalDeviceQueueFamilyProperties
            getPhysicalDeviceQueueFamilyProperties_{};
        PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties_{};
        PFN_vkGetPhysicalDeviceFeatures2 getPhysicalDeviceFeatures2_{};
        PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties_{};
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR getPhysicalDeviceSurfaceSupportKHR_{};
        PFN_vkCreateDevice createDevice_{};
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
            getPhysicalDeviceSurfaceCapabilitiesKHR_{};
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getPhysicalDeviceSurfaceFormatsKHR_{};
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
            getPhysicalDeviceSurfacePresentModesKHR_{};
        // NOLINTEND

        instance_base() = default;
        constexpr explicit instance_base(VkInstance instance) noexcept
            : value_{instance},
              destroyDebugUtilsMessengerEXT_{funPtr<PFN_vkDestroyDebugUtilsMessengerEXT>(
                  "vkDestroyDebugUtilsMessengerEXT")},
              createDebugUtilsMessengerEXT_{funPtr<PFN_vkCreateDebugUtilsMessengerEXT>(
                  "vkCreateDebugUtilsMessengerEXT")},
              destroySurfaceKHR_{funPtr<PFN_vkDestroySurfaceKHR>("vkDestroySurfaceKHR")},
              enumeratePhysicalDevices_{
                  funPtr<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices")},
              getPhysicalDeviceProperties_{funPtr<PFN_vkGetPhysicalDeviceProperties>(
                  "vkGetPhysicalDeviceProperties")},
              getPhysicalDeviceQueueFamilyProperties_{
                  funPtr<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
                      "vkGetPhysicalDeviceQueueFamilyProperties")},
              enumerateDeviceExtensionProperties_{
                  funPtr<PFN_vkEnumerateDeviceExtensionProperties>(
                      "vkEnumerateDeviceExtensionProperties")},
              getPhysicalDeviceFeatures2_{funPtr<PFN_vkGetPhysicalDeviceFeatures2>(
                  "vkGetPhysicalDeviceFeatures2")},
              getPhysicalDeviceMemoryProperties_{
                  funPtr<PFN_vkGetPhysicalDeviceMemoryProperties>(
                      "vkGetPhysicalDeviceMemoryProperties")},
              getPhysicalDeviceSurfaceSupportKHR_{
                  funPtr<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
                      "vkGetPhysicalDeviceSurfaceSupportKHR")},
              createDevice_{funPtr<PFN_vkCreateDevice>("vkCreateDevice")},
              getPhysicalDeviceSurfaceCapabilitiesKHR_{
                  funPtr<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
                      "vkGetPhysicalDeviceSurfaceCapabilitiesKHR")},
              getPhysicalDeviceSurfaceFormatsKHR_{
                  funPtr<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
                      "vkGetPhysicalDeviceSurfaceFormatsKHR")},
              getPhysicalDeviceSurfacePresentModesKHR_{
                  funPtr<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
                      "vkGetPhysicalDeviceSurfacePresentModesKHR")}
        {
            MCSLOG_INFO("load instance pfn [begin]");
            MCS_ASSERT(instance != nullptr);
            MCS_ASSERT(destroyDebugUtilsMessengerEXT_ != nullptr);
            MCS_ASSERT(createDebugUtilsMessengerEXT_ != nullptr);
            MCS_ASSERT(destroySurfaceKHR_ != nullptr);
            MCS_ASSERT(enumeratePhysicalDevices_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceProperties_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceQueueFamilyProperties_ != nullptr);
            MCS_ASSERT(enumerateDeviceExtensionProperties_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceFeatures2_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceMemoryProperties_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceSurfaceSupportKHR_ != nullptr);
            MCS_ASSERT(createDevice_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceSurfaceCapabilitiesKHR_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceSurfaceFormatsKHR_ != nullptr);
            MCS_ASSERT(getPhysicalDeviceSurfacePresentModesKHR_ != nullptr);
            MCSLOG_INFO("load instance pfn [end]");
        }
    };

}; // namespace mcs::vulkan::core