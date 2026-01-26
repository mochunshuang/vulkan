#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

#include "PhysicalDevice.hpp"
#include "./utils/make_vk_exception.hpp"

namespace mcs::vulkan::core
{
    struct select_queue_family_index
    {
        using check_VkQueueFamilyProperties = std::function<bool(
            const VkQueueFamilyProperties &qfp, uint32_t queueFamilyIndex)>;
        using select_callback =
            std::function<uint32_t(const std::vector<uint32_t> &candidate)>;

        constexpr explicit select_queue_family_index(
            PhysicalDevice &physicalDevice) noexcept
            : queueFamilies_{physicalDevice.getQueueFamilyProperties()},
              candidate_(queueFamilies_.size())
        {
        }

        constexpr auto &requiredQueueFamily(
            check_VkQueueFamilyProperties checkQueueFamilyFun) noexcept
        {
            checkQueueFamily_ = std::move(checkQueueFamilyFun);
            return *this;
        }

        auto &check()
        {
            candidate_.clear();
            if (checkQueueFamily_.has_value())
            {
                for (uint32_t i = 0, queueFamilyCount = queueFamilies_.size();
                     i < queueFamilyCount; i++)
                {
                    if ((*checkQueueFamily_)(queueFamilies_[i], i))
                    {
                        candidate_.emplace_back(i);
                    }
                }
            }
            if (candidate_.empty())
                throw make_vk_exception("failed to find a suitable family index.");
            return *this;
        }
        auto &select(const select_callback &fn)
        {
            queueFamilyIndex_ = fn(candidate_);
            return *this;
        }
        [[nodiscard]] auto create() const
        {
            if (queueFamilyIndex_ == ~0)
                throw make_vk_exception("not call select.");
            return queueFamilyIndex_;
        }

      private:
        std::optional<check_VkQueueFamilyProperties> checkQueueFamily_;
        std::vector<VkQueueFamilyProperties> queueFamilies_;
        std::vector<uint32_t> candidate_;
        uint32_t queueFamilyIndex_ = ~0;
    };

}; // namespace mcs::vulkan::core
