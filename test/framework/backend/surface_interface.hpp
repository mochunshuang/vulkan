#pragma once

#include <exception>
#include <print>
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan::core
{

    struct surface_interface
    {
        surface_interface(const surface_interface &) = default;
        surface_interface(surface_interface &&) = default;
        surface_interface &operator=(const surface_interface &) = default;
        surface_interface &operator=(surface_interface &&) = default;
        constexpr virtual ~surface_interface() = default;

        constexpr explicit surface_interface(VkSurfaceKHR surface) noexcept
            : surface_{surface}
        {
        }

        [[nodiscard]] constexpr VkSurfaceKHR surface() const noexcept
        {
            return surface_;
        };

        // impl
        [[nodiscard]] constexpr virtual VkExtent2D chooseSwapExtent(
            const VkSurfaceCapabilitiesKHR & /*capabilities*/) const noexcept
        {
            std::println("terminate: not override function chooseSwapExtent");
            std::terminate();
        }
        constexpr virtual void waitGoodFramebufferSize() const noexcept
        {
            std::println("terminate: not override function waitGoodFramebufferSize");
            std::terminate();
        }

      private:
        VkSurfaceKHR surface_;
    };

}; // namespace mcs::vulkan::core
