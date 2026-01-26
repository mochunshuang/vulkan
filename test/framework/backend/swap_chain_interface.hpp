#pragma once

#include "LogicalDevice.hpp"
#include "choose_swap_present_mode.hpp"
#include "choose_swap_surface_format.hpp"
#include "sType.hpp"
#include "surface_interface.hpp"
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan::core
{
    // NOTE: 需要重构,太垃圾了
    struct swap_chain_interface
    {
        constexpr swap_chain_interface(const LogicalDevice &device,
                                       std::unique_ptr<surface_interface> surface)
            : logicalDevice_{&device}, surface_{std::move(surface)}
        {
        }
        [[nodiscard]] constexpr auto *logicalDevice() const noexcept
        {
            return logicalDevice_;
        }
        [[nodiscard]] constexpr VkSurfaceKHR surface() const noexcept
        {
            return surface_->surface();
        }
        [[nodiscard]] constexpr auto getSurfaceCapabilitiesKHR() const
        {
            return logicalDevice_->physicalDevice()->getSurfaceCapabilitiesKHR(surface());
        }
        [[nodiscard]] constexpr auto getSurfaceFormatsKHR() const
        {
            return logicalDevice_->physicalDevice()->getSurfaceFormatsKHR(surface());
        }
        [[nodiscard]] constexpr auto getSurfacePresentModesKHR() const
        {
            return logicalDevice_->physicalDevice()->getSurfacePresentModesKHR(surface());
        }

        enum class MinImageCountStrategy : std::uint8_t
        {
            MINIMUM_REQUIRED, // 使用最小必需数量
            MINIMUM_PLUS_ONE, // 最小+1（推荐的平衡策略）
            MINIMUM_PLUS_TWO  // 最小+2
        };

      public:
        struct config_swapchain // NOLINTBEGIN
        {
            /*
            typedef struct VkSwapchainCreateInfoKHR {
                VkStructureType                  sType;
                const void*                      pNext;
                VkSwapchainCreateFlagsKHR        flags;

                VkSurfaceKHR                     surface;
                uint32_t                         minImageCount;

                VkFormat                         imageFormat;
                VkColorSpaceKHR                  imageColorSpace;
                VkExtent2D                       imageExtent;

                uint32_t                         imageArrayLayers;
                VkImageUsageFlags                imageUsage;
                VkSharingMode                    imageSharingMode;
                uint32_t                         queueFamilyIndexCount;
                const uint32_t*                  pQueueFamilyIndices;
                VkSurfaceTransformFlagBitsKHR    preTransform;
                VkCompositeAlphaFlagBitsKHR      compositeAlpha;
                VkPresentModeKHR                 presentMode;
                VkBool32                         clipped;
                VkSwapchainKHR                   oldSwapchain;
            } VkSwapchainCreateInfoKHR;
            */
            VkSwapchainCreateInfoKHR create(swap_chain_interface *self)
            {
                const auto CAPABILITIES = self->getSurfaceCapabilitiesKHR();

                VkSurfaceKHR surface = self->surface();

                const auto MIN_COUNT = CAPABILITIES.minImageCount;
                auto minImageCount = MIN_COUNT;
                if (minImageCountStrategy == MinImageCountStrategy::MINIMUM_PLUS_ONE)
                    minImageCount = MIN_COUNT + 1;
                else
                    minImageCount = MIN_COUNT + 2;
                if (CAPABILITIES.maxImageCount > 0 &&
                    minImageCount > CAPABILITIES.maxImageCount)
                    minImageCount = CAPABILITIES.maxImageCount;

                auto [imageFormat, imageColorSpace] = choose_swap_surface_format(
                    chooseSurfaceFormat, self->getSurfaceFormatsKHR());

                self->imageFormat_ = imageFormat;
                self->imageExtent_ = self->surface_->chooseSwapExtent(CAPABILITIES);
                VkExtent2D imageExtent = self->imageExtent_;

                VkPresentModeKHR presentMode = choose_swap_present_mode(
                    choosePresentMode, self->getSurfacePresentModesKHR());
                VkSurfaceTransformFlagBitsKHR preTransform =
                    CAPABILITIES.currentTransform;

                return {.sType = sType<VkSwapchainCreateInfoKHR>(),
                        .pNext = pNext,
                        .flags = static_cast<VkSwapchainCreateFlagsKHR>(flags),
                        .surface = surface,
                        .minImageCount = minImageCount,
                        .imageFormat = imageFormat,
                        .imageColorSpace = imageColorSpace,
                        .imageExtent = imageExtent,
                        .imageArrayLayers = imageArrayLayers,
                        .imageUsage = static_cast<VkImageUsageFlags>(imageUsage),
                        .imageSharingMode = imageSharingMode,
                        .queueFamilyIndexCount =
                            static_cast<uint32_t>(queueFamilyIndices.size()),
                        .pQueueFamilyIndices = queueFamilyIndices.data(),
                        .preTransform = preTransform,
                        .compositeAlpha = compositeAlpha,
                        .presentMode = presentMode,
                        .clipped = clipped,
                        .oldSwapchain = oldSwapchain};
            }
            const void *pNext{};
            VkSwapchainCreateFlagBitsKHR flags{};
            MinImageCountStrategy minImageCountStrategy{};

            VkSurfaceFormatKHR chooseSurfaceFormat{};

            uint32_t imageArrayLayers{};
            VkImageUsageFlagBits imageUsage{};
            VkSharingMode imageSharingMode{};

            std::vector<uint32_t> queueFamilyIndices;

            VkCompositeAlphaFlagBitsKHR compositeAlpha{};
            VkPresentModeKHR choosePresentMode{};
            VkBool32 clipped{};
            VkSwapchainKHR oldSwapchain{};
        }; // NOLINTEND

        struct config_image_view // NOLINTBEGIN
        {
            /*
            typedef struct VkImageViewCreateInfo {
                VkStructureType            sType;
                const void*                pNext;
                VkImageViewCreateFlags     flags;
                VkImage                    image;
                VkImageViewType            viewType;
                VkFormat                   format;
                VkComponentMapping         components;
                VkImageSubresourceRange    subresourceRange;
            } VkImageViewCreateInfo;
            */
            VkImageViewCreateInfo create()
            {
                return {.sType = sType<VkImageViewCreateInfo>(),
                        .pNext = pNext,
                        .flags = static_cast<VkImageViewCreateFlags>(flags),
                        .viewType = viewType,
                        .components = components,
                        .subresourceRange = subresourceRange};
            }
            const void *pNext{};
            VkImageViewCreateFlagBits flags{};
            VkImageViewType viewType{};
            VkComponentMapping components{};
            VkImageSubresourceRange subresourceRange{};
        }; // NOLINTEND

        auto &configSwapchain(config_swapchain swapchain) noexcept
        {
            configSwapchain_ = std::move(swapchain);
            return *this;
        }
        auto &configImageView(config_image_view image_view) noexcept
        {
            configImageView_ = std::move(image_view); // NOLINT
            return *this;
        }

      private:
        void createSwapChain()
        {
            VkSwapchainCreateInfoKHR createInfo = configSwapchain_.create(this);

            swapChain_ = logicalDevice_->createSwapchainKHR(createInfo,
                                                            logicalDevice_->allocator());
            swapChainImages_ = logicalDevice_->getSwapchainImagesKHR(swapChain_);

            createImageViews(swapChainImages_);
        }

        void createImageViews(const std::vector<VkImage> &swapChainImages)
        {

            VkImageViewCreateInfo createInfo = configImageView_.create();
            createInfo.format = imageFormat_;

            for (auto *image : swapChainImages)
            {
                createInfo.image = image;
                swapChainImageViews_.emplace_back(
                    logicalDevice_->createImageView(createInfo, nullptr));
            }
        }

      public:
        void clear()
        {
            for (auto *imageView : swapChainImageViews_)
                logicalDevice_->destroyImageView(imageView, logicalDevice_->allocator());
            if (swapChain_ != nullptr)
                logicalDevice_->destroySwapchainKHR(swapChain_,
                                                    logicalDevice_->allocator());

            swapChainImageViews_.clear();
            swapChain_ = nullptr;
        }
        constexpr void waitGoodFramebufferSize()
        {
            surface_->waitGoodFramebufferSize();
        }
        auto recreate()
        {
            createSwapChain();
        }

        [[nodiscard]] auto create()
        {
            createSwapChain();
            return std::move(*this);
        }

        [[nodiscard]] const VkFormat &refImageFormat() const noexcept
        {
            return imageFormat_;
        }
        [[nodiscard]] auto swapChainImagesSize() const noexcept
        {
            return swapChainImages_.size();
        }
        [[nodiscard]] auto getImage(size_t idx) const noexcept
        {
            return swapChainImages_[idx];
        }
        [[nodiscard]] auto getImageView(size_t idx) const noexcept
        {
            return swapChainImageViews_[idx];
        }

        [[nodiscard]] VkExtent2D imageExtent() const noexcept
        {
            return imageExtent_;
        }
        auto acquireNextImage(uint64_t timeout, VkSemaphore semaphore,
                              VkFence fence) const noexcept
        {
            return logicalDevice_->acquireNextImageKHR(swapChain_, timeout, semaphore,
                                                       fence);
        }

        constexpr VkSwapchainKHR &operator*() noexcept
        {
            return swapChain_;
        }
        constexpr const VkSwapchainKHR &operator*() const noexcept
        {
            return swapChain_;
        }

      private:
        const LogicalDevice *logicalDevice_;
        std::unique_ptr<surface_interface> surface_;

        VkSwapchainKHR swapChain_{};
        std::vector<VkImage> swapChainImages_;
        std::vector<VkImageView> swapChainImageViews_;

        VkExtent2D imageExtent_{}; // NOTE: 伴随窗口更新
        VkFormat imageFormat_{};   // NOTE:管线需要

        config_swapchain configSwapchain_;
        config_image_view configImageView_;
    };

}; // namespace mcs::vulkan::core