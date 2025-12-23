#pragma once

#include "logical_device.hpp"
#include <cstdint>
#include <utility>

namespace mcs::vulkan
{
    struct command_buffer
    {
        using value_type = VkCommandBuffer;

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return commandBuffer_ != nullptr;
        }
        [[nodiscard]] value_type raw_data() const noexcept // NOLINT
        {
            return commandBuffer_;
        }
        [[nodiscard]] VkCommandPool commandPool() const noexcept
        {
            return commandPool_;
        }

        [[nodiscard]] uint32_t count() const noexcept
        {
            return count_;
        }
        [[nodiscard]] auto *device() const noexcept
        {
            return device_;
        }

        command_buffer(const logical_device &device,
                       const VkCommandBufferAllocateInfo &allocInfo)
            : commandBuffer_{device.allocateCommandBuffers(allocInfo)},
              commandPool_(allocInfo.commandPool), count_{allocInfo.commandBufferCount},
              device_(&device)
        {
        }
        constexpr ~command_buffer() noexcept
        {
            destroy();
        }
        constexpr command_buffer(command_buffer &&other) noexcept
            : commandBuffer_(std::exchange(other.commandBuffer_, {})),
              commandPool_(std::exchange(other.commandPool_, {})),
              count_(std::exchange(other.count_, {})),
              device_{std::exchange(other.device_, {})}
        {
        }
        constexpr command_buffer &operator=(command_buffer &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                commandBuffer_ = std::exchange(other.commandBuffer_, {});
                commandPool_ = std::exchange(other.commandPool_, {});
                count_ = std::exchange(other.count_, {});
                device_ = std::exchange(other.device_, {});
            }
            return *this;
        }
        command_buffer(const command_buffer &) = delete;
        command_buffer &operator=(const command_buffer &) = delete;

      private:
        value_type commandBuffer_ = {};
        VkCommandPool commandPool_ = {};
        uint32_t count_ = {};
        const logical_device *device_ = {};

        constexpr void destroy() noexcept
        {
            if (device_ != nullptr)
            {
                device_->freeCommandBuffers(commandPool_, count_, commandBuffer_);
                commandBuffer_ = {};
                commandPool_ = {};
                count_ = {};
                device_ = {};
            }
        }
    };

}; // namespace mcs::vulkan