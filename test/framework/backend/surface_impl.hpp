#pragma once

#include "surface_interface.hpp"

namespace mcs::vulkan::core
{
    template <typename Impl>
    struct surface_impl : surface_interface
    {
        constexpr surface_impl(Impl &window, VkSurfaceKHR surface) noexcept
            : surface_interface{surface}, window_{&window}
        {
        }

        [[nodiscard]] constexpr VkExtent2D chooseSwapExtent(
            const VkSurfaceCapabilitiesKHR &capabilities) const noexcept override
        {
            return window_->chooseSwapExtent(capabilities);
        }
        constexpr void waitGoodFramebufferSize() const noexcept override
        {
            window_->waitGoodFramebufferSize();
        }

      private:
        Impl *window_;
    };

}; // namespace mcs::vulkan::core