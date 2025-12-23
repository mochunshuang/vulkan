#pragma once

#include "logical_device.hpp"

#include <functional>
#include <optional>
#include <utility>

#include "./utils/vk_exception.hpp"
#include "sType.hpp"

#include "make_queue_family_index.hpp"

namespace mcs::vulkan
{

    struct make_logical_device
    {
        using check_queueFamily_properties_type =
            bool(const VkQueueFamilyProperties &qfp, uint32_t queueFamilyIndex,
                 const physical_device &physicalDevice);

        using after_build_success_type = void(const logical_device &logicalDevice,
                                              uint32_t queueFamilyIndex);

        using receive_queue_family_index_type = bool(uint32_t queueFamilyIndex);

        using modify_queue_create_info_type =
            void(VkDeviceQueueCreateInfo &queueCreateInfo);
        using device_createInfo_type =
            VkDeviceCreateInfo(VkDeviceQueueCreateInfo &queueCreateInfo);

        auto &setQueuePriority(float queuePriority) noexcept
        {
            queuePriority_ = queuePriority;
            return *this;
        }

        template <typename Fn>
        constexpr auto &afterQueueCreateInfoInit(Fn &&modifyQueueCreateInfoFun) noexcept
        {
            modifyQueueCreateInfo_ = std::forward<Fn>(modifyQueueCreateInfoFun);
            return *this;
        }
        template <typename Fn>
        constexpr auto &requiredDeviceCreateInfo(Fn &&deviceCreateInfoFun) noexcept
        {
            deviceCreateInfo_ = std::forward<Fn>(deviceCreateInfoFun);
            return *this;
        }

        template <typename Fn>
        auto &afterBuildSuccess(Fn &&afterBuildSuccessFun)
        {
            afterBuildSuccess_ = std::forward<Fn>(afterBuildSuccessFun);
            return *this;
        }

        constexpr auto build(make_queue_family_index queue_family)
        {
            auto &physicalDevice_ = queue_family.physicalDevice_;

            if (not deviceCreateInfo_.has_value())
                throw utils::make_vk_exception("device createInfo function not set.");

            uint32_t queueFamilyIndex = queue_family.build();

            VkDeviceQueueCreateInfo queueCreateInfo = {
                .sType = sType<VkDeviceQueueCreateInfo>(),
                .queueFamilyIndex = queueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority_};
            if (modifyQueueCreateInfo_.has_value())
                (*modifyQueueCreateInfo_)(queueCreateInfo);

            VkDeviceCreateInfo createInfo = (*deviceCreateInfo_)(queueCreateInfo);
            VkDevice device = physicalDevice_.createDevice(&createInfo, nullptr);

            logical_device logicalDevice{device};
            if (afterBuildSuccess_.has_value())
                (*afterBuildSuccess_)(logicalDevice, queueFamilyIndex);
            return logicalDevice;
        }

      private:
        float queuePriority_ = 1.0F;

        std::optional<std::function<modify_queue_create_info_type>>
            modifyQueueCreateInfo_;
        std::optional<std::function<device_createInfo_type>> deviceCreateInfo_;
        std::optional<std::function<after_build_success_type>> afterBuildSuccess_;
    };

}; // namespace mcs::vulkan