#pragma once

#include "DescriptorPool.hpp"
#include <utility>
#include <vector>
#include <ranges>

namespace mcs::vulkan::core
{
    class DescriptorSet
    {
        using value_type = VkDescriptorSet;
        const DescriptorPool *descriptorPool_{};
        value_type value_;

      public:
        constexpr explicit operator bool() const noexcept
        {
            return value_ != nullptr;
        }
        constexpr value_type &operator*() noexcept
        {
            return value_;
        }
        constexpr const value_type &operator*() const noexcept
        {
            return value_;
        }

        using create_info = DescriptorPool::create_descriptor_sets_info;
        constexpr DescriptorSet(const DescriptorPool *descriptorPool, value_type value)
            : descriptorPool_{descriptorPool}, value_{value}
        {
        }
        constexpr ~DescriptorSet() noexcept
        {
            clear();
        }
        DescriptorSet(const DescriptorSet &) = delete;
        constexpr DescriptorSet(DescriptorSet &&o) noexcept
            : descriptorPool_{std::exchange(o.descriptorPool_, {})},
              value_{std::exchange(o.value_, {})}
        {
        }
        DescriptorSet &operator=(const DescriptorSet &) = delete;
        constexpr DescriptorSet &operator=(DescriptorSet &&o) noexcept
        {
            if (&o != this)
            {
                this->clear();
                descriptorPool_ = std::exchange(o.descriptorPool_, {});
                value_ = std::exchange(o.value_, {});
            }
            return *this;
        }

        constexpr void clear() noexcept
        {
            if (value_ != nullptr)
            {
                descriptorPool_->freeDescriptorSets(1, &value_);
                descriptorPool_ = nullptr;
                value_ = nullptr;
            }
        }
    };

    [[nodiscard]] constexpr std::vector<DescriptorSet> DescriptorPool::
        allocateDescriptorSets(const create_descriptor_sets_info &createInfo) const
    {
        return logicalDevice()->allocateDescriptorSets(createInfo.create(value_)) |
               std::views::transform([this](VkDescriptorSet v) constexpr noexcept {
                   return DescriptorSet{this, v};
               }) |
               std::ranges::to<std::vector<DescriptorSet>>();
    }

}; // namespace mcs::vulkan::core