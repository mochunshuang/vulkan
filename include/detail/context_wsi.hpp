#pragma once

#include "context_base.hpp"

#include "choose_swap_extent.hpp"
#include "choose_swap_surface_format.hpp"
#include "choose_swap_present_mode.hpp"
#include "choose_swap_min_image_count.hpp"
#include "utils/mcs_assert.hpp"

namespace mcs::vulkan
{
    template <typename surface_impl>
    struct context_wsi
    {
        constexpr context_wsi(context_base &ctx, surface_impl &surface) noexcept
            : contextBase_{&ctx}, surfaceImpl_{&surface}
        {
            mcs::MCS_ASSERT(ctx.surface() != nullptr);
        }

        [[nodiscard]] constexpr auto *contextBase() const noexcept
        {
            return contextBase_;
        }
        [[nodiscard]] constexpr auto *surfaceImpl() const noexcept
        {
            return surfaceImpl_;
        }
        [[nodiscard]] decltype(auto) surfaceExtent() const noexcept
        {
            return choose_swap_extent(*surfaceImpl(), getSurfaceCapabilitiesKHR());
        }
        [[nodiscard]] decltype(auto) chooseSurfaceFormat(
            const VkSurfaceFormatKHR &select) const noexcept
        {
            return choose_swap_surface_format(select, getSurfaceFormatsKHR());
        }
        [[nodiscard]] decltype(auto) choosePresentMode(
            VkPresentModeKHR select) const noexcept
        {
            return choose_swap_present_mode(select, getSurfacePresentModesKHR());
        }
        [[nodiscard]] decltype(auto) minImageCount() const noexcept
        {
            return choose_swap_min_image_count(getSurfaceCapabilitiesKHR());
        }

        [[nodiscard]] VkSurfaceKHR surface() const noexcept
        {
            return contextBase_->surface();
        }

        [[nodiscard]] VkInstance raw_instance() const noexcept // NOLINT
        {
            return contextBase_->raw_instance();
        }
        [[nodiscard]] decltype(auto) ref_instance() const & noexcept // NOLINT
        {
            return contextBase_->ref_instance();
        }

        [[nodiscard]] VkPhysicalDevice raw_physical_device() const noexcept // NOLINT
        {
            return contextBase_->raw_physical_device();
        }
        [[nodiscard]] decltype(auto) ref_physical_device() const & noexcept // NOLINT
        {
            return contextBase_->ref_physical_device();
        }

        [[nodiscard]] VkDevice raw_logical_device() const noexcept // NOLINT
        {
            return contextBase_->raw_logical_device();
        }
        [[nodiscard]] auto &ref_logical_device() const & noexcept // NOLINT
        {
            return contextBase_->ref_logical_device();
        }

        [[nodiscard]] VkQueue defaultQueue() const noexcept
        {
            return contextBase_->defaultQueue();
        }
        [[nodiscard]] VkCommandPool defalutCommandPool() const noexcept
        {
            return contextBase_->defalutCommandPool();
        }

        [[nodiscard]] decltype(auto) getSurfaceCapabilitiesKHR() const noexcept
        {
            return contextBase_->getSurfaceCapabilitiesKHR();
        }
        [[nodiscard]] decltype(auto) getSurfaceFormatsKHR() const noexcept
        {
            return contextBase_->getSurfaceFormatsKHR();
        }
        [[nodiscard]] decltype(auto) getMaxUsableSampleCount() const noexcept
        {
            return contextBase_->getMaxUsableSampleCount();
        }

        [[nodiscard]] decltype(auto) getSurfacePresentModesKHR() const noexcept
        {
            return contextBase_->getSurfacePresentModesKHR();
        }

      private:
        context_base *contextBase_;
        surface_impl *surfaceImpl_;
    };

}; // namespace mcs::vulkan