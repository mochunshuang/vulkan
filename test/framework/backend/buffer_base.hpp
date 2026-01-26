#pragma once

#include "LogicalDevice.hpp"
#include <utility>

namespace mcs::vulkan::core
{
    struct buffer_base
    {
        buffer_base() = default;
        constexpr buffer_base(const LogicalDevice &device, VkBuffer buffer,
                              VkDeviceMemory bufferMemory) noexcept
            : device_{&device}, buffer_{buffer}, bufferMemory_{bufferMemory}
        {
        }
        constexpr ~buffer_base() noexcept
        {
            destroy();
        }
        buffer_base(const buffer_base &) = delete;
        buffer_base &operator=(const buffer_base &) = delete;

        constexpr buffer_base(buffer_base &&other) noexcept
            : device_{std::exchange(other.device_, nullptr)},
              buffer_{std::exchange(other.buffer_, nullptr)},
              bufferMemory_{std::exchange(other.bufferMemory_, nullptr)}
        {
        }

        constexpr buffer_base &operator=(buffer_base &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                device_ = std::exchange(other.device_, nullptr);
                buffer_ = std::exchange(other.buffer_, nullptr);
                bufferMemory_ = std::exchange(other.bufferMemory_, nullptr);
            }
            return *this;
        }

        [[nodiscard]] constexpr VkBuffer buffer() const noexcept
        {
            return buffer_;
        }
        [[nodiscard]] constexpr VkDeviceMemory bufferMemory() const noexcept
        {
            return bufferMemory_;
        }

        [[nodiscard]] constexpr auto *device() const noexcept
        {
            return device_;
        }
        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return device_ != nullptr && buffer_ != nullptr && bufferMemory_ != nullptr;
        }

        [[nodiscard]] auto *map(size_t size) const
        {
            void *data; // NOLINT
            device_->mapMemory(bufferMemory_, 0, size, 0, &data);
            return data;
        }
        void unmap() const noexcept
        {
            device_->unmapMemory(bufferMemory_);
        }
        void copyDataToBuffer(const void *src, size_t size,
                              VkMemoryMapFlags flags = 0) const
        {
            void *data; // NOLINT
            device_->mapMemory(bufferMemory(), 0, size, flags, &data);
            ::memcpy(data, src, size);
            device_->unmapMemory(bufferMemory());
        }
        void clear()
        {
            destroy();
        }

      private:
        const LogicalDevice *device_{};
        VkBuffer buffer_{};
        VkDeviceMemory bufferMemory_{};

        constexpr void destroy() noexcept
        {
            if (device_ != nullptr)
            {
                if (buffer_ != nullptr)
                    device_->destroyBuffer(buffer_, device_->allocator());
                if (bufferMemory_ != nullptr)
                    device_->freeMemory(bufferMemory_, device_->allocator());

                device_ = nullptr;
                buffer_ = nullptr;
                bufferMemory_ = nullptr;
            }
        }
    };

}; // namespace mcs::vulkan::core
