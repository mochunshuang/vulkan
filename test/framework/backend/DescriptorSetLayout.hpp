#pragma once

#include "LogicalDevice.hpp"
#include "sType.hpp"
#include <utility>
#include <vector>

namespace mcs::vulkan::core
{
    class DescriptorSetLayout
    {
        using value_type = VkDescriptorSetLayout;
        const LogicalDevice *logicalDevice_{};
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
        /*
        typedef struct VkDescriptorSetLayoutCreateInfo {
            VkStructureType                        sType;
            const void*                            pNext;
            VkDescriptorSetLayoutCreateFlags       flags;
            uint32_t                               bindingCount;
            const VkDescriptorSetLayoutBinding*    pBindings;
        } VkDescriptorSetLayoutCreateInfo;
        */
        struct create_info // NOLINTBEGIN
        {
            VkDescriptorSetLayoutCreateInfo create() const noexcept
            {
                return {.sType = sType<VkDescriptorSetLayoutCreateInfo>(),
                        .pNext = pNext,
                        .flags = static_cast<VkDescriptorSetLayoutCreateFlags>(flags),
                        .bindingCount = static_cast<uint32_t>(bindings.size()),
                        .pBindings = bindings.data()};
            }
            const void *pNext{};
            VkDescriptorSetLayoutCreateFlagBits flags{};
            std::vector<VkDescriptorSetLayoutBinding> bindings;
        }; // NOLINTEND

        constexpr DescriptorSetLayout(const LogicalDevice &device,
                                      const create_info &createInfo)
            : logicalDevice_{&device}, value_{device.createDescriptorSetLayout(
                                           createInfo.create(), device.allocator())}
        {
        }
        constexpr ~DescriptorSetLayout() noexcept
        {
            clear();
        }
        DescriptorSetLayout(const DescriptorSetLayout &) = delete;
        constexpr DescriptorSetLayout(DescriptorSetLayout &&o) noexcept
            : logicalDevice_{std::exchange(o.logicalDevice_, {})},
              value_{std::exchange(o.value_, {})}
        {
        }
        DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;
        constexpr DescriptorSetLayout &operator=(DescriptorSetLayout &&o) noexcept
        {
            if (&o != this)
            {
                this->clear();
                logicalDevice_ = std::exchange(o.logicalDevice_, {});
                value_ = std::exchange(o.value_, {});
            }
            return *this;
        }

        constexpr void clear() noexcept
        {
            if (value_ != nullptr)
            {
                logicalDevice_->destroyDescriptorSetLayout(value_,
                                                           logicalDevice_->allocator());
                logicalDevice_ = nullptr;
                value_ = nullptr;
            }
        }
    };

}; // namespace mcs::vulkan::core