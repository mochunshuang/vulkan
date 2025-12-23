#pragma once

#include "logical_device.hpp"
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

namespace mcs::vulkan
{
    struct swap_chain
    {
        swap_chain() = default;
        swap_chain(const logical_device &device, VkSwapchainKHR swapChain,
                   std::vector<VkImage> &&swapChainImages,
                   std::vector<VkImageView> &&swapChainImageViews) noexcept
            : device_{&device}, swapChain_{swapChain},
              swapChainImages_{std::move(swapChainImages)},
              swapChainImageViews_{std::move(swapChainImageViews)}
        {
        }

        [[nodiscard]] auto swapChainKHR() const noexcept
        {
            return swapChain_;
        }
        [[nodiscard]] auto &ref_swapChainImages() const noexcept // NOLINT
        {
            return swapChainImages_;
        }

        [[nodiscard]] auto &ref_swapChainImageViews() const noexcept // NOLINT
        {
            return swapChainImageViews_;
        }
        [[nodiscard]] auto *device() const noexcept
        {
            return device_;
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return device_ != nullptr && swapChain_ != nullptr &&
                   !swapChainImages_.empty() && !swapChainImageViews_.empty();
        }

        constexpr ~swap_chain() noexcept
        {
            destroy();
        }
        swap_chain(swap_chain &&other) noexcept
            : device_{std::exchange(other.device_, {})},
              swapChain_{std::exchange(other.swapChain_, {})},
              swapChainImages_{std::exchange(other.swapChainImages_, {})},
              swapChainImageViews_{std::exchange(other.swapChainImageViews_, {})}
        {
        }
        swap_chain &operator=(swap_chain &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                device_ = std::exchange(other.device_, {});
                swapChain_ = std::exchange(other.swapChain_, {});
                swapChainImages_ = std::exchange(other.swapChainImages_, {});
                swapChainImageViews_ = std::exchange(other.swapChainImageViews_, {});
            }
            return *this;
        };

        swap_chain(const swap_chain &) = delete;
        swap_chain &operator=(const swap_chain &) = delete;

      private:
        const logical_device *device_ = nullptr;
        VkSwapchainKHR swapChain_ = nullptr;
        std::vector<VkImage> swapChainImages_;
        std::vector<VkImageView> swapChainImageViews_; // NOLINTEND

        constexpr void destroy() noexcept
        {
            if (device_ != nullptr)
            {
                for (auto *imageView : swapChainImageViews_)
                    device_->destroyImageView(imageView, nullptr);
                if (swapChain_ != nullptr)
                    device_->destroySwapchainKHR(swapChain_);

                device_ = nullptr;
                swapChainImageViews_.clear();
                swapChain_ = nullptr;
            }
        }
    };
}; // namespace mcs::vulkan