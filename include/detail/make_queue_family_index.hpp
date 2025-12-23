#pragma once

#include "physical_device.hpp"

#include <functional>
#include <optional>

#include "./utils/vk_exception.hpp"

namespace mcs::vulkan
{
    struct make_logical_device;

    struct make_queue_family_index
    {
        using check_queueFamily_properties_type =
            bool(const VkQueueFamilyProperties &qfp, uint32_t queueFamilyIndex,
                 const physical_device &physicalDevice);

        constexpr explicit make_queue_family_index(
            physical_device physicalDevice) noexcept
            : physicalDevice_{physicalDevice}
        {
        }

        template <typename Fn>
        constexpr auto &requiredQueueFamilyProperties(Fn &&checkQueueFamilyFun) noexcept
        {
            checkQueueFamilyProperties_ = std::forward<Fn>(checkQueueFamilyFun);
            return *this;
        }

        auto build()
        {
            uint32_t queueFamilyIndex = UINT32_MAX;
            if (checkQueueFamilyProperties_.has_value())
            {
                std::vector<VkQueueFamilyProperties> queueFamilies =
                    physicalDevice_.getQueueFamilyProperties();
                for (uint32_t i = 0, queueFamilyCount = queueFamilies.size();
                     i < queueFamilyCount; i++)
                {
                    if ((*checkQueueFamilyProperties_)(queueFamilies[i], i,
                                                       physicalDevice_))
                    {
                        queueFamilyIndex = i;
                        break;
                    }
                }
            }
            if (queueFamilyIndex == UINT32_MAX)
                throw utils::make_vk_exception("check queueFamily propertiesfailure.");

            return queueFamilyIndex;
        }

      private:
        std::optional<std::function<check_queueFamily_properties_type>>
            checkQueueFamilyProperties_;
        physical_device physicalDevice_;

        friend struct make_logical_device;
    };

}; // namespace mcs::vulkan