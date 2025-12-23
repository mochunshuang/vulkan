#pragma once

#include "image_base.hpp"
#include <utility>

namespace mcs::vulkan
{
    struct deep_image : image_base
    {
        [[nodiscard]] VkFormat getDeepFormat() const noexcept
        {
            return deepFormat_;
        }

        constexpr deep_image() = default;
        constexpr deep_image(image_base base, VkFormat deepFormat) noexcept
            : image_base{std::move(base)}, deepFormat_{deepFormat}
        {
        }

      private:
        VkFormat deepFormat_{};
    };
}; // namespace mcs::vulkan