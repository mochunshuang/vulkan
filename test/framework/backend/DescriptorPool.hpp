#pragma once

#include "LogicalDevice.hpp"
#include "sType.hpp"
#include <utility>
#include <vector>

#include "VkFlagsWrapper.hpp"

namespace mcs::vulkan::core
{
    class DescriptorSet;
    class DescriptorPool
    {
        using value_type = VkDescriptorPool;
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
        [[nodiscard]] const LogicalDevice *logicalDevice() const noexcept
        {
            return logicalDevice_;
        }
        /*
       typedef struct VkDescriptorPoolCreateInfo {
           VkStructureType                sType;
           const void*                    pNext;
           VkDescriptorPoolCreateFlags    flags;
           uint32_t                       maxSets;
           uint32_t                       poolSizeCount;
           const VkDescriptorPoolSize*    pPoolSizes;
       } VkDescriptorPoolCreateInfo;
       */
        struct create_info // NOLINTBEGIN
        {
            static_assert(sizeof(VkFlagsWrapper<VkDescriptorPoolCreateFlagBits>) ==
                              sizeof(VkDescriptorPoolCreateFlags),
                          "枚举包装失败");

            VkDescriptorPoolCreateInfo create() const noexcept
            {
                return {.sType = sType<VkDescriptorPoolCreateInfo>(),
                        .pNext = pNext,
                        .flags = flags,
                        .maxSets = maxSets,
                        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                        .pPoolSizes = poolSizes.data()};
            }
            const void *pNext{};
            VkFlagsWrapper<VkDescriptorPoolCreateFlagBits> flags{
                VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT};
            uint32_t maxSets{};
            std::vector<VkDescriptorPoolSize> poolSizes;
        }; // NOLINTEND

        constexpr DescriptorPool(const LogicalDevice &device,
                                 const create_info &createInfo)
            : logicalDevice_{&device},
              value_{device.createDescriptorPool(createInfo.create(), device.allocator())}
        {
        }
        constexpr ~DescriptorPool() noexcept
        {
            clear();
        }
        DescriptorPool(const DescriptorPool &) = delete;
        constexpr DescriptorPool(DescriptorPool &&o) noexcept
            : logicalDevice_{std::exchange(o.logicalDevice_, {})},
              value_{std::exchange(o.value_, {})}
        {
        }
        DescriptorPool &operator=(const DescriptorPool &) = delete;
        constexpr DescriptorPool &operator=(DescriptorPool &&o) noexcept
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
                logicalDevice_->destroyDescriptorPool(value_,
                                                      logicalDevice_->allocator());
                logicalDevice_ = nullptr;
                value_ = nullptr;
            }
        }

        /*
       typedef struct VkDescriptorSetAllocateInfo {
           VkStructureType                 sType;
           const void*                     pNext;
           VkDescriptorPool                descriptorPool;
           uint32_t                        descriptorSetCount;
           const VkDescriptorSetLayout*    pSetLayouts;
       } VkDescriptorSetAllocateInfo;
       */
        struct create_descriptor_sets_info // NOLINTBEGIN
        {
            VkDescriptorSetAllocateInfo create(
                VkDescriptorPool descriptorPool) const noexcept
            {
                return {.sType = sType<VkDescriptorSetAllocateInfo>(),
                        .pNext = pNext,
                        .descriptorPool = descriptorPool,
                        .descriptorSetCount =
                            static_cast<uint32_t>(descriptorSets.size()),
                        .pSetLayouts = descriptorSets.data()};
            }
            const void *pNext{};
            uint32_t maxSets{};
            std::vector<VkDescriptorSetLayout> descriptorSets;
        }; // NOLINTEND
        [[nodiscard]] constexpr std::vector<DescriptorSet> allocateDescriptorSets(
            const create_descriptor_sets_info &info) const;
        void freeDescriptorSets(uint32_t descriptorSetCount,
                                const VkDescriptorSet *pDescriptorSets) const noexcept
        {
            logicalDevice_->freeDescriptorSets(value_, descriptorSetCount,
                                               pDescriptorSets);
        }
    };

}; // namespace mcs::vulkan::core