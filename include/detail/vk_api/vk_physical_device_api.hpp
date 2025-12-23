#pragma once

#include "../utils/check_vk_result.hpp"

#include <vector>

namespace mcs::vulkan::vk_api
{
    struct vk_physical_device_api
    {
        [[nodiscard]] static auto enumeratePhysicalDevices(VkInstance instance)
        {
            uint32_t deviceCount; // NOLINT
            utils::check_vk_result(
                ::vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
            if (deviceCount == 0)
                throw std::runtime_error("failed to find GPUs with Vulkan support!");
            std::vector<VkPhysicalDevice> gpus(deviceCount);
            utils::check_vk_result(
                ::vkEnumeratePhysicalDevices(instance, &deviceCount, gpus.data()));
            return gpus;
        }
        [[nodiscard]] static auto getPhysicalDeviceQueueFamilyProperties(
            VkPhysicalDevice gpu)
        {
            uint32_t queueFamilyCount = 0;
            ::vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            ::vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount,
                                                       queueFamilies.data());
            return queueFamilies;
        }
        [[nodiscard]] static auto enumerateDeviceExtensionProperties(VkPhysicalDevice gpu)
        {
            // Check if all required device extensions are available
            uint32_t extensionCount; // NOLINT
            utils::check_vk_result(::vkEnumerateDeviceExtensionProperties(
                gpu, nullptr, &extensionCount, nullptr));
            std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
            utils::check_vk_result(::vkEnumerateDeviceExtensionProperties(
                gpu, nullptr, &extensionCount, availableDeviceExtensions.data()));
            return availableDeviceExtensions;
        }
        [[nodiscard]] static auto getPhysicalDeviceProperties(
            VkPhysicalDevice gpu) noexcept
        {
            VkPhysicalDeviceProperties deviceProperties;
            ::vkGetPhysicalDeviceProperties(gpu, &deviceProperties);
            return deviceProperties;
        }
        [[nodiscard]] static auto getPhysicalDeviceFormatProperties(
            VkPhysicalDevice gpu, VkFormat format) noexcept
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(gpu, format, &formatProperties);
            return formatProperties;
        }
        [[nodiscard]] static bool getPhysicalDeviceSurfaceSupportKHR(
            VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
            VkSurfaceKHR surface)
        {
            VkBool32 presentSupport; // NOLINT
            utils::check_vk_result(::vkGetPhysicalDeviceSurfaceSupportKHR(
                physicalDevice, queueFamilyIndex, surface, &presentSupport));
            return presentSupport == VK_TRUE;
        }

        static void getPhysicalDeviceFeatures2(
            VkPhysicalDevice gpu, VkPhysicalDeviceFeatures2 *pFeatures) noexcept
        {
            vkGetPhysicalDeviceFeatures2(gpu, pFeatures);
        }

        constexpr static auto getPhysicalDeviceSurfaceCapabilitiesKHR(
            VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) noexcept
        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                        &surfaceCapabilities);
            return surfaceCapabilities;
        }
        constexpr static auto getPhysicalDeviceSurfaceFormatsKHR(
            VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
        {
            uint32_t formatCount; // NOLINT
            utils::check_vk_result(::vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice, surface, &formatCount, nullptr));
            std::vector<VkSurfaceFormatKHR> formats(formatCount);
            utils::check_vk_result(::vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice, surface, &formatCount, formats.data()));
            return formats;
        }
        constexpr static auto getPhysicalDeviceSurfacePresentModesKHR(
            VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
        {
            uint32_t presentModeCount; // NOLINT
            utils::check_vk_result(::vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, nullptr));
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            utils::check_vk_result(::vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, presentModes.data()));
            return presentModes;
        }
    };

}; // namespace mcs::vulkan::vk_api