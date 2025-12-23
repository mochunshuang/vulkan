#pragma once

#include "swap_chain.hpp"
#include "utils/vk_exception.hpp"

#include <utility>

namespace mcs::vulkan
{
    template <typename context_type>
    struct make_swap_chain
    {
        using swapchain_create_info_type =
            VkSwapchainCreateInfoKHR(make_swap_chain *self);
        using image_view_create_info_type = VkImageViewCreateInfo(
            const VkImage &image, const VkSwapchainCreateInfoKHR &createInfo);

        constexpr explicit make_swap_chain(context_type &contextBase) noexcept
            : context_(&contextBase)
        {
        }

        [[nodiscard]] const VkExtent2D &ref_surfaceExtent() const noexcept // NOLINT
        {
            return surfaceExtent_;
        }
        auto &setSurfaceExtent(const VkExtent2D &surfaceExtent) noexcept
        {
            surfaceExtent_ = surfaceExtent;
            return *this;
        }
        template <typename Fn>
        auto &requiredSwapchainCreateInfoKHR(Fn &&swapchainCreateInfoFn) noexcept
        {
            swapchainCreateInfoFn_ = std::forward<Fn>(swapchainCreateInfoFn);
            return *this;
        }

        template <typename Fn>
        auto &requiredImageViewCreateInfo(Fn &&imageViewCreateInfoFn) noexcept
        {
            imageViewCreateInfoFn_ = std::forward<Fn>(imageViewCreateInfoFn);
            return *this;
        }

        auto build()
        {
            if (swapchainCreateInfoFn_ == nullptr)
                throw utils::make_vk_exception("swapchainCreateInfo function not set.");
            if (imageViewCreateInfoFn_ == nullptr)
                throw utils::make_vk_exception("imageViewCreateInfo function not set.");

            const auto &device = context_->ref_logical_device();

            VkSwapchainKHR swapChain = nullptr;
            std::vector<VkImageView> swapChainImageViews;
            try
            {
                // 1. createSwapChain
                VkSwapchainCreateInfoKHR swapchainCreateInfo =
                    (*swapchainCreateInfoFn_)(this);
                swapChain = device.createSwapchainKHR(swapchainCreateInfo);
                std::vector<VkImage> swapChainImages =
                    device.getSwapchainImagesKHR(swapChain);

                // 2. createImageViews
                swapChainImageViews.resize(swapChainImages.size());
                for (size_t i = 0; i < swapChainImages.size(); i++)
                {
                    VkImage image = swapChainImages[i];
                    VkImageViewCreateInfo createInfo =
                        (*imageViewCreateInfoFn_)(image, swapchainCreateInfo);
                    swapChainImageViews[i] = device.createImageView(createInfo, nullptr);
                }

                return swap_chain{device, swapChain, std::move(swapChainImages),
                                  std::move(swapChainImageViews)};
            }
            catch (...)
            {
                for (auto *imageView : swapChainImageViews)
                    device.destroyImageView(imageView, nullptr);
                if (swapChain != nullptr)
                    device.destroySwapchainKHR(swapChain);
                throw;
            }
        }

        [[nodiscard]] auto *context() const noexcept
        {
            return context_;
        }

      private:
        context_type *context_{}; // NOLINT

        swapchain_create_info_type *swapchainCreateInfoFn_{};
        image_view_create_info_type *imageViewCreateInfoFn_{};

        VkExtent2D surfaceExtent_{};
    };

}; // namespace mcs::vulkan