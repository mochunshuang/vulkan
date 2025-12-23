#pragma once

#include "./physical_device.hpp"
#include <functional>
#include <optional>

#include <algorithm>
#include <utility>

namespace mcs::vulkan
{
    struct make_physical_device
    {
        using check_device_properties_type =
            bool(const VkPhysicalDeviceProperties &device_properties);
        using check_queueFamily_properties_type =
            bool(const VkQueueFamilyProperties &qfp);
        using check_features_type = bool(const physical_device &device);

        constexpr auto &requiredDeviceProperties(auto &&fun) noexcept
        {
            checkDeviceProperties_ = std::forward<decltype(fun)>(fun);
            return *this;
        }
        constexpr auto &requiredQueueFamilyProperties(auto &&fun) noexcept
        {
            checkQueueFamilyProperties_ = std::forward<decltype(fun)>(fun);
            return *this;
        }
        constexpr auto &requiredDeviceExtensions(
            const std::vector<const char *> &requiredDeviceExtension)
        {
            requiredDeviceExtension_ = requiredDeviceExtension;
            return *this;
        }
        constexpr auto &requiredFeatures(auto &&fun) noexcept
        {
            checkFeatures_ = std::forward<decltype(fun)>(fun);
            return *this;
        }

        constexpr physical_device pickPhysicalDevice()
        {
            const auto IT = std::ranges::find_if(
                availablePhysicalDevices_, [&](const VkPhysicalDevice &device) -> bool {
                    physical_device physicalDevice{device};

                    // Check device properties
                    if (checkDeviceProperties_.has_value() &&
                        not(*checkDeviceProperties_)(physicalDevice.getProperties()))
                        return false;

                    // Check if any of the queue families support
                    if (checkQueueFamilyProperties_.has_value() &&
                        not std::ranges::any_of(physicalDevice.getQueueFamilyProperties(),
                                                *checkQueueFamilyProperties_))
                        return false;

                    // Check if all required device extensions are available
                    if (not requiredDeviceExtension_.empty() &&
                        not checkDeviceExtensions(physicalDevice))
                        return false;

                    // Query for Vulkan 1.3 features
                    if (checkFeatures_.has_value() &&
                        not(*checkFeatures_)(physicalDevice))
                        return false;
                    return true;
                });
            return IT != availablePhysicalDevices_.end() ? physical_device{*IT}
                                                         : physical_device{};
        }

        constexpr void updateAvailablePhysicalDevices(
            const std::vector<VkPhysicalDevice> &availablePhysicalDevices) noexcept
        {
            availablePhysicalDevices_ = availablePhysicalDevices;
        }
        constexpr std::vector<VkPhysicalDevice> &
        ref_availablePhysicalDevices() noexcept // NOLINT
        {
            return availablePhysicalDevices_;
        }

        constexpr explicit make_physical_device(VkInstance instance)
            : availablePhysicalDevices_{
                  physical_device::availablePhysicalDevices(instance)}
        {
        }

      private:
        std::vector<VkPhysicalDevice> availablePhysicalDevices_;
        std::optional<std::function<check_device_properties_type>> checkDeviceProperties_;
        std::optional<std::function<check_queueFamily_properties_type>>
            checkQueueFamilyProperties_;
        std::vector<const char *> requiredDeviceExtension_;
        std::optional<std::function<check_features_type>> checkFeatures_;

        [[nodiscard]] constexpr bool checkDeviceExtensions(
            const physical_device &device) const
        {
            const auto AVAILABLE_DEVICE_EXTENSIONS =
                device.availableDeviceExtensionProperties();

            return std::ranges::all_of(
                requiredDeviceExtension_, [&](const char *requiredExtension) noexcept {
                    return std::ranges::any_of(
                        AVAILABLE_DEVICE_EXTENSIONS,
                        [&](const VkExtensionProperties &availableExtension) noexcept {
                            return ::strcmp(availableExtension.extensionName,
                                            requiredExtension) == 0;
                        });
                });
        }
    };

}; // namespace mcs::vulkan