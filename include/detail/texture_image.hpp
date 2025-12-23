#pragma once

#include "image_base.hpp"
#include "utils/mcs_assert.hpp"
#include <utility>

namespace mcs::vulkan
{
    struct texture_image : image_base
    {
        [[nodiscard]] VkSampler sampler() const noexcept
        {
            return sampler_;
        }
        [[nodiscard]] bool valid() const noexcept
        {
            return sampler_ != nullptr;
        }
        constexpr texture_image() = default;
        constexpr texture_image(image_base base, VkSampler sampler) noexcept
            : image_base{std::move(base)}, sampler_{sampler}
        {
            MCS_ASSERT(device() != nullptr);
        }

        constexpr ~texture_image()
        {
            destroy();
        }
        texture_image(const texture_image &) = delete;
        texture_image &operator=(const texture_image &other) noexcept;

        constexpr texture_image(texture_image &&other) noexcept
            : image_base{std::move(other)}, sampler_{std::exchange(other.sampler_, {})}
        {
        }

        constexpr texture_image &operator=(texture_image &&other) noexcept
        {
            if (this != &other)
            {
                this->destroy();
                static_cast<image_base &>(*this) = std::move(other);
                sampler_ = std::exchange(other.sampler_, {});
            }
            return *this;
        }

      private:
        VkSampler sampler_ = nullptr;

        constexpr void destroy() noexcept
        {
            if (valid())
            {
                device()->destroySampler(sampler_);
                sampler_ = nullptr;
            }
        }
    };
}; // namespace mcs::vulkan