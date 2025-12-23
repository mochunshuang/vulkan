#pragma once

#include "utils/vk_exception.hpp"
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan
{
    template <typename Derived, typename context_type>
    struct make_image_base
    {

        using image_create_info_type = VkImageCreateInfo(Derived *self);
        using image_view_create_info_type = VkImageViewCreateInfo(
            const VkImageCreateInfo &imageCreateInfo, const VkImage &image);

        Derived &as_derived() noexcept // NOLINT
        {
            return *static_cast<Derived *>(this);
        }

        constexpr explicit make_image_base(context_type &ctx) noexcept : context_{&ctx} {}

        constexpr auto &requiredMemoryProperties(
            const VkMemoryPropertyFlags &properties) noexcept
        {
            properties_ = properties;
            return as_derived();
        }

        template <typename Fn>
        constexpr auto &requiredImageCreateInfo(Fn &&imageCreateInfoFun) noexcept
        {
            imageCreateInfoFn_ = std::forward<Fn>(imageCreateInfoFun);
            return as_derived();
        }

        template <typename Fn>
        constexpr auto &requiredImageViewCreateInfo(Fn &&imageViewCreateInfoFun) noexcept
        {
            imageViewCreateInfoFn_ = std::forward<Fn>(imageViewCreateInfoFun);
            return as_derived();
        }

        void ensureValid() const
        {
            if (imageCreateInfoFn_ == nullptr)
                throw utils::make_vk_exception(
                    "requiredImageCreateInfo function not set.");
            if (imageViewCreateInfoFn_ == nullptr)
                throw utils::make_vk_exception(
                    "requiredImageViewCreateInfo function not set.");
        }

        [[nodiscard]] auto *context() const noexcept
        {
            return context_;
        }

      protected:
        auto &imageCreateInfoFn() noexcept
        {
            return imageCreateInfoFn_;
        }

        auto &imageViewCreateInfoFn() noexcept
        {
            return imageViewCreateInfoFn_;
        }

        [[nodiscard]] VkMemoryPropertyFlags properties() noexcept
        {
            return properties_;
        }

        // make_image_base() = default; // NOTE: 私有化将影响到移动语义

      private:
        context_type *context_{};
        image_create_info_type *imageCreateInfoFn_{};
        image_view_create_info_type *imageViewCreateInfoFn_{};

        VkMemoryPropertyFlags properties_{};
    };

}; // namespace mcs::vulkan