#pragma once

#include "logical_device.hpp"
#include "utils/mcs_assert.hpp"
#include <utility>

namespace mcs::vulkan
{
    struct image_base
    {
        [[nodiscard]] constexpr auto valid() const noexcept
        {
            return image_ != nullptr && imageMemory_ != nullptr && imageView_ != nullptr;
        }

        constexpr image_base() = default;
        constexpr image_base(const logical_device &device, VkImage image,
                             VkDeviceMemory imageMemory, VkImageView imageView) noexcept
            : device_{&device}, image_{image}, imageMemory_{imageMemory},
              imageView_{imageView}
        {
            MCS_ASSERT(valid());
        }
        constexpr ~image_base() noexcept
        {
            destroy();
        }

        constexpr image_base(image_base &&other) noexcept
            : device_{std::exchange(other.device_, {})},
              image_{std::exchange(other.image_, {})},
              imageMemory_{std::exchange(other.imageMemory_, {})},
              imageView_{std::exchange(other.imageView_, {})}
        {
        }

        constexpr image_base &operator=(image_base &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                device_ = std::exchange(other.device_, nullptr);
                image_ = std::exchange(other.image_, nullptr);
                imageMemory_ = std::exchange(other.imageMemory_, nullptr);
                imageView_ = std::exchange(other.imageView_, nullptr);
            }
            return *this;
        }
        image_base(const image_base &) = delete;
        image_base &operator=(const image_base &) = delete;

        [[nodiscard]] constexpr VkImage image() const noexcept
        {
            return image_;
        }
        [[nodiscard]] constexpr VkDeviceMemory imageMemory() const noexcept
        {
            return imageMemory_;
        }
        [[nodiscard]] constexpr VkImageView imageView() const noexcept
        {
            return imageView_;
        }

        [[nodiscard]] auto &device() const noexcept
        {
            return device_;
        }

      private:
        const logical_device *device_ = nullptr;
        VkImage image_ = nullptr;
        VkDeviceMemory imageMemory_ = nullptr;
        VkImageView imageView_ = nullptr;

        constexpr void destroy() noexcept
        {
            if (device_ != nullptr)
            {
                if (imageView_ != nullptr)
                {
                    device_->destroyImageView(imageView_);
                    imageView_ = nullptr;
                }
                if (image_ != nullptr)
                {
                    device_->destroyImage(image_, nullptr);
                    image_ = nullptr;
                }
                if (imageMemory_ != nullptr)
                {
                    device_->freeMemory(imageMemory_, nullptr);
                    imageMemory_ = nullptr;
                }
                device_ = nullptr;
            }
        }
    };

}; // namespace mcs::vulkan