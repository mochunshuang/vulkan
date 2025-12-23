#pragma once

#include "logical_device.hpp"
#include "sType.hpp"
#include "utils/mcs_assert.hpp"
#include <utility>

namespace mcs::vulkan
{
    struct descriptor_resource
    {
        [[nodiscard]] auto valid() const noexcept
        {
            return descriptorPool_ != nullptr && descriptorSetLayout_ != nullptr;
        }

        constexpr descriptor_resource() = default;
        constexpr descriptor_resource(const logical_device &device,
                                      VkDescriptorPool descriptorPool,
                                      VkDescriptorSetLayout descriptorSetLayout) noexcept
            : device_{&device}, descriptorPool_{descriptorPool},
              descriptorSetLayout_{descriptorSetLayout}
        {
            MCS_ASSERT(valid());
        }

        [[nodiscard]] constexpr auto *device() const noexcept
        {
            return device_;
        }
        [[nodiscard]] constexpr VkDescriptorPool descriptorPool() const noexcept
        {
            return descriptorPool_;
        }
        [[nodiscard]] constexpr VkDescriptorSetLayout descriptorSetLayout() const noexcept
        {
            return descriptorSetLayout_;
        }

        [[nodiscard]] constexpr std::vector<VkDescriptorSet> allocateDescriptorSets(
            size_t size) const
        {
            std::vector<VkDescriptorSetLayout> layouts{size, descriptorSetLayout_};

            VkDescriptorSetAllocateInfo allocInfo = {
                .sType = sType<VkDescriptorSetAllocateInfo>(),
                .descriptorPool = descriptorPool_,
                .descriptorSetCount = static_cast<uint32_t>(size),
                .pSetLayouts = layouts.data()};
            return device_->allocateDescriptorSets(allocInfo);
        }

        constexpr ~descriptor_resource() noexcept
        {
            destroy();
        }
        constexpr descriptor_resource(descriptor_resource &&other) noexcept
            : device_{std::exchange(other.device_, {})},
              descriptorPool_{std::exchange(other.descriptorPool_, {})},
              descriptorSetLayout_{std::exchange(other.descriptorSetLayout_, {})}
        {
        }
        constexpr descriptor_resource &operator=(descriptor_resource &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                device_ = std::exchange(other.device_, {});
                descriptorPool_ = std::exchange(other.descriptorPool_, {});
                descriptorSetLayout_ = std::exchange(other.descriptorSetLayout_, {});
            }
            return *this;
        }
        descriptor_resource(const descriptor_resource &) = delete;
        descriptor_resource &operator=(const descriptor_resource &) = delete;

      private:
        const logical_device *device_{};
        VkDescriptorPool descriptorPool_{};
        VkDescriptorSetLayout descriptorSetLayout_{};

        constexpr void destroy() noexcept
        {
            if (device_ != nullptr)
            {
                if (descriptorSetLayout_ != nullptr)
                    device_->destroyDescriptorSetLayout(descriptorSetLayout_, nullptr);
                if (descriptorPool_ != nullptr)
                    device_->destroyDescriptorPool(descriptorPool_, nullptr);
                descriptorSetLayout_ = {};
                descriptorPool_ = {};
                device_ = {};
            }
        }
    };

}; // namespace mcs::vulkan