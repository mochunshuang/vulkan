#pragma once

#include "LogicalDevice.hpp"
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan::core
{
    class Queue
    {
        using value_type = VkQueue;

        const LogicalDevice *device_{};
        value_type value_{};

      public:
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

        Queue() = default;
        constexpr Queue(const LogicalDevice &device, VkQueue value) noexcept
            : device_{&device}, value_{value}
        {
        }
        constexpr void submit(uint32_t submitCount, const VkSubmitInfo &submits,
                              VkFence fence) const
        {
            device_->queueSubmit(value_, submitCount, submits, fence);
        }
        [[nodiscard]] constexpr VkResult presentKHR(
            const VkPresentInfoKHR &presentInfo) const
        {
            return device_->queuePresentKHR(value_, presentInfo);
        }
        constexpr void waitIdle() const noexcept
        {
            device_->queueWaitIdle(value_);
        }
    };
} // namespace mcs::vulkan::core