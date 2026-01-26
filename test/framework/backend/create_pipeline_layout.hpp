#pragma once

#include <vector>

#include "sType.hpp"

#include "LogicalDevice.hpp"

namespace mcs::vulkan::core
{
    struct create_pipeline_layout
    {

        explicit create_pipeline_layout(const LogicalDevice &device) noexcept
            : logicalDevice_{&device}
        {
        }

        constexpr auto &pNext(const void *pNext) noexcept
        {
            createInfo_.pNext = pNext;
            return *this;
        }
        constexpr auto &flags(VkPipelineLayoutCreateFlagBits flags) noexcept
        {
            createInfo_.flags = flags;
            return *this;
        }

        constexpr auto &setLayouts(std::vector<VkDescriptorSetLayout> setLayouts) noexcept
        {
            setLayouts_ = std::move(setLayouts);
            return *this;
        }

        constexpr auto &pushConstantRanges(
            std::vector<VkPushConstantRange> pushConstantRanges) noexcept
        {
            pushConstantRanges_ = std::move(pushConstantRanges);
            return *this;
        }

        [[nodiscard]] VkPipelineLayout create()
        {
            createInfo_.setLayoutCount = setLayouts_.size();
            createInfo_.pSetLayouts = setLayouts_.data();
            createInfo_.pushConstantRangeCount = pushConstantRanges_.size();
            createInfo_.pPushConstantRanges = pushConstantRanges_.data();
            return logicalDevice_->createPipelineLayout(createInfo_,
                                                        logicalDevice_->allocator());
        }

      private:
        const LogicalDevice *logicalDevice_{};

        /*
        typedef struct VkPipelineLayoutCreateInfo {
            VkStructureType                 sType;
            const void*                     pNext;
            VkPipelineLayoutCreateFlags     flags;
            uint32_t                        setLayoutCount;
            const VkDescriptorSetLayout*    pSetLayouts;
            uint32_t                        pushConstantRangeCount;
            const VkPushConstantRange*      pPushConstantRanges;
        } VkPipelineLayoutCreateInfo;
        */
        VkPipelineLayoutCreateInfo createInfo_{.sType =
                                                   sType<VkPipelineLayoutCreateInfo>()};
        std::vector<VkDescriptorSetLayout> setLayouts_;
        std::vector<VkPushConstantRange> pushConstantRanges_;
    };
} // namespace mcs::vulkan::core