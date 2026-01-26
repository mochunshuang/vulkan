#pragma once

#include <functional>
#include <print>
#include <vector>

#include "app_context.hpp"
#include "PhysicalDevice.hpp"
#include "sType.hpp"
#include "utils/make_vk_exception.hpp"

#include "LogicalDevice.hpp"

namespace mcs::vulkan::core
{
    struct create_logical_device
    {
        using create_callback = std::function<void(LogicalDevice &logicalDevice)>;

        explicit create_logical_device(app_context &ctx, PhysicalDevice &device) noexcept
            : ctx_{&ctx}, physicalDevice_{&device}
        {
        }

        auto &setFlags(const VkDeviceCreateFlags &flags)
        {
            flags_ = flags;
            return *this;
        }

        auto &setQueueCreateInfos(
            const std::vector<VkDeviceQueueCreateInfo> &queueCreateInfos)
        {
            queueCreateInfos_ = queueCreateInfos;
            return *this;
        }

        auto &setEnableFeatureChain(
            VkPhysicalDeviceFeatures2 *enablefeatureChainHead) noexcept
        {
            enablefeatureChain_ = enablefeatureChainHead;
            return *this;
        }
        auto &setEnabledExtension(const std::vector<const char *> &enabledExtension)
        {
            enabledExtension_ = enabledExtension;
            return *this;
        }

        LogicalDevice create(const create_callback &fn)
        {
            if (enablefeatureChain_ == nullptr || queueCreateInfos_.empty() ||
                enabledExtension_.empty())
                throw make_vk_exception("need set more data of VkDeviceCreateInfo.");

            VkDeviceCreateInfo createInfo_{
                .sType = sType<VkDeviceCreateInfo>(),
                .pNext = enablefeatureChain_,
                .flags = flags_,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos_.size()),
                .pQueueCreateInfos = queueCreateInfos_.data(),
                .enabledExtensionCount = static_cast<uint32_t>(enabledExtension_.size()),
                .ppEnabledExtensionNames = enabledExtension_.data()};
            auto logical_device = LogicalDevice{
                physicalDevice_->createDevice(&createInfo_, ctx_->allocator()),
                ctx_->allocator(), physicalDevice_};

            try
            {
                fn(logical_device);

                return logical_device;
            }
            catch (...)
            {
                logical_device.destroyDevice(ctx_->allocator());
                throw;
            }
        }

      private:
        /*
       typedef struct VkDeviceCreateInfo {
           VkStructureType                    sType;
           const void*                        pNext;
           VkDeviceCreateFlags                flags;
           uint32_t                           queueCreateInfoCount;
           const VkDeviceQueueCreateInfo*     pQueueCreateInfos;
           // enabledLayerCount is deprecated and should not be used
           uint32_t                           enabledLayerCount;
           // ppEnabledLayerNames is deprecated and should not be used
           const char* const*                 ppEnabledLayerNames;
           uint32_t                           enabledExtensionCount;
           const char* const*                 ppEnabledExtensionNames;
           const VkPhysicalDeviceFeatures*    pEnabledFeatures;
       } VkDeviceCreateInfo;
       */
        app_context *ctx_;
        PhysicalDevice *physicalDevice_;

        VkDeviceCreateFlags flags_{};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos_;
        void *enablefeatureChain_{};
        std::vector<const char *> enabledExtension_;
    };
} // namespace mcs::vulkan::core