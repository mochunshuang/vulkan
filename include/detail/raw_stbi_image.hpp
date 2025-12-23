#pragma once

#include "utils/mcs_assert.hpp"
#include <utility>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "utils/vk_exception.hpp"
#include "get_mip_levels.hpp"

namespace mcs::vulkan
{
    struct raw_stbi_image
    {
        raw_stbi_image() = default;
        constexpr explicit raw_stbi_image(const char *filename)
        {
            stbi_uc *pixels =
                ::stbi_load(filename, &width_, &height_, &channels_, STBI_rgb_alpha);
            if (pixels == nullptr)
                throw utils::vk_exception("failed to load texture image!");

            data_ = pixels;
            static_assert(STBI_rgb_alpha == 4); // 4个通道
            imageSize_ = width_ * height_ * STBI_rgb_alpha;

            MCS_ASSERT(width_ != 0);
            MCS_ASSERT(height_ != 0);
        }
        constexpr ~raw_stbi_image() noexcept
        {
            destroy();
        }

        constexpr raw_stbi_image(raw_stbi_image &&other) noexcept
            : data_{std::exchange(other.data_, {})},
              width_{std::exchange(other.width_, {})},
              height_{std::exchange(other.height_, {})},
              channels_{std::exchange(other.channels_, {})},
              imageSize_{std::exchange(other.imageSize_, {})} {};

        constexpr raw_stbi_image &operator=(raw_stbi_image &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                data_ = std::exchange(other.data_, {});
                width_ = std::exchange(other.width_, {});
                height_ = std::exchange(other.height_, {});
                channels_ = std::exchange(other.channels_, {});
                imageSize_ = std::exchange(other.imageSize_, {});
            }
            return *this;
        }
        raw_stbi_image(const raw_stbi_image &) = delete;
        raw_stbi_image &operator=(const raw_stbi_image &) = delete;

        [[nodiscard]] bool valid() const noexcept
        {
            return data_ != nullptr;
        }

        [[nodiscard]] int width() const noexcept
        {
            return width_;
        }

        [[nodiscard]] int height() const noexcept
        {
            return height_;
        }

        [[nodiscard]] int channels() const noexcept
        {
            return channels_;
        }

        [[nodiscard]] int imageSize() const noexcept
        {
            return imageSize_;
        }

        [[nodiscard]] unsigned char *data() const noexcept
        {
            return data_;
        }

        [[nodiscard]] auto mipLevels() const noexcept
        {
            return get_mip_levels(width(), height());
        }

      private:
        unsigned char *data_ = nullptr;
        int width_{};
        int height_{};
        int channels_{};
        int imageSize_{};

        constexpr void destroy() noexcept
        {
            if (data_ != nullptr)
                stbi_image_free(data_);
        }
    };
}; // namespace mcs::vulkan