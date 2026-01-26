#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <print>
#include <utility>
#include <vector>
#include <ranges>

#include "Instance.hpp"
#include "app_context.hpp"
#include "PhysicalDevice.hpp"
#include "utils/to_string.hpp"
#include "./utils/make_vk_exception.hpp"

namespace mcs::vulkan::core
{

    struct pick_physical_device
    {
        using check_VkPhysicalDeviceProperties =
            std::function<bool(const VkPhysicalDeviceProperties &)>;
        using check_VkQueueFamilyProperties =
            std::function<bool(const VkQueueFamilyProperties &qfp)>;
        using check_features = std::function<bool(const PhysicalDevice &device)>;
        using pick_callback =
            std::function<size_t(const std::vector<PhysicalDevice *> &candidate)>;

      private:
        app_context *ctx_;
        std::vector<PhysicalDevice> devices_; // 一开始是全部,后来
        std::vector<size_t> candidate_;
        size_t selectIndex_{static_cast<size_t>(~0)};

        // check
        std::optional<check_VkPhysicalDeviceProperties> requiredProperties_;
        std::optional<check_VkQueueFamilyProperties> requiredQueueFamily_;
        std::vector<const char *> requiredDeviceExtension_;
        std::optional<check_features> requiredFeatures_;

        [[nodiscard]] constexpr bool checkDeviceExtensions(
            const PhysicalDevice &device) const
        {
            const auto AVAILABLE_DEVICE_EXTENSIONS =
                device.enumerateDeviceExtensionProperties();

            return std::ranges::all_of(
                requiredDeviceExtension_, [&](const char *requiredExtension) noexcept {
                    return std::ranges::any_of(
                        AVAILABLE_DEVICE_EXTENSIONS,
                        [&](const VkExtensionProperties &availableExtension) noexcept {
                            return ::strcmp(availableExtension.extensionName,
                                            requiredExtension) == 0;
                        });
                });
        }

      public:
        explicit pick_physical_device(app_context &ctx) : ctx_{&ctx}
        {
            auto gpus = ctx_->instance().enumeratePhysicalDevices();
            for (auto &gpu : gpus)
                devices_.emplace_back(ctx.instance(), gpu);
            candidate_.resize(devices_.size());
        }
        auto &reset() noexcept
        {
            selectIndex_ = 0;
            return *this;
        }
        auto &requiredProperties(
            std::optional<check_VkPhysicalDeviceProperties> requiredProperties)
        {
            requiredProperties_ = std::move(requiredProperties);
            return *this;
        }
        auto &requiredQueueFamily(
            std::optional<check_VkQueueFamilyProperties> requiredQueueFamily)
        {
            requiredQueueFamily_ = std::move(requiredQueueFamily);
            return *this;
        }
        auto &requiredDeviceExtension(std::vector<const char *> requiredDeviceExtension)
        {
            requiredDeviceExtension_ = std::move(requiredDeviceExtension);
            return *this;
        }
        auto &requiredFeatures(std::optional<check_features> requiredFeatures)
        {
            requiredFeatures_ = std::move(requiredFeatures);
            return *this;
        }

        auto &check()
        {
            candidate_.clear();
            for (size_t i = 0, size = devices_.size(); i < size; ++i)
            {
                const PhysicalDevice &physicalDevice = devices_[i];

                // Check device properties
                if (requiredProperties_.has_value() &&
                    not(*requiredProperties_)(physicalDevice.getProperties()))
                    continue;

                // Check if any of the queue families support
                if (requiredQueueFamily_.has_value() &&
                    not std::ranges::any_of(physicalDevice.getQueueFamilyProperties(),
                                            *requiredQueueFamily_))
                    continue;

                // Check if all required device extensions are available
                if (not requiredDeviceExtension_.empty() &&
                    not checkDeviceExtensions(physicalDevice))
                    continue;

                // Query for Vulkan 1.3 features
                if (requiredFeatures_.has_value() &&
                    not(*requiredFeatures_)(physicalDevice))
                    continue;

                candidate_.emplace_back(i);
            }
            if (candidate_.empty())
                throw make_vk_exception("failed to find a suitable GPU!.");
            return *this;
        }

        auto &pickIndex(const pick_callback &pickFn)
        {
            if (devices_.size() == 1)
            {
                selectIndex_ = pickFn(std::vector<PhysicalDevice *>{devices_.data()});
                return *this;
            }
            const auto CANDIDATE_DEVICES =
                candidate_ | std::views::transform([this](size_t idx) constexpr noexcept {
                    return &devices_[idx];
                }) |
                std::ranges::to<std::vector<PhysicalDevice *>>();
            selectIndex_ = pickFn(CANDIDATE_DEVICES);
            return *this;
        }

        auto create()
        {
            if (selectIndex_ == ~0)
                throw make_vk_exception("not call pickIndex.");
            print();
            return std::make_pair(selectIndex_, devices_[selectIndex_]);
        }

        // NOLINTBEGIN
        void print()
        {
            std::println("\npick_physical_device: [begin]");
            std::println("selectIndex: {}", selectIndex_);
            std::println("candidate size: {}", candidate_.size());
            std::println("total size: {}", devices_.size());

            const PhysicalDevice &device = devices_[selectIndex_];
            std::println("\n=== PhysicalDeviceProperties ===");
            std::println("{}", to_string(device.getProperties()));

            std::println("\n=== Queue Families ===");
            const auto QUEUE_FAMILIES = device.getQueueFamilyProperties();
            for (size_t i = 0; i < QUEUE_FAMILIES.size(); ++i)
            {
                const auto &qf = QUEUE_FAMILIES[i];
                std::println("  Queue Family {}:", i);
                std::println("    Queue Count: {}", qf.queueCount);
                std::println("    Queue Flags: {}", // NOTE: 需要转化,因为用int32抽象
                             to_string(static_cast<VkQueueFlagBits>(qf.queueFlags)));
                std::println("    Timestamp Valid Bits: {}", qf.timestampValidBits);
                std::println("    Min Image Transfer Granularity: {}",
                             to_string(qf.minImageTransferGranularity));
            }

            const auto MEMORY_PROPS = device.getMemoryProperties();

            // 打印内存信息
            std::println("\n=== Memory Properties ===");
            std::println("Memory Heap Count: {}", MEMORY_PROPS.memoryHeapCount);
            for (uint32_t i = 0; i < MEMORY_PROPS.memoryHeapCount; ++i)
            {
                std::println("  Heap {}: Size = {} MB, Flags = {}", i,
                             MEMORY_PROPS.memoryHeaps[i].size / (1024 * 1024),
                             to_string(static_cast<VkMemoryHeapFlagBits>(
                                 MEMORY_PROPS.memoryHeaps[i].flags)));
            }

            std::println("Memory Type Count: {}", MEMORY_PROPS.memoryTypeCount);
            for (uint32_t i = 0; i < MEMORY_PROPS.memoryTypeCount; ++i)
            {
                std::println("  Type {}: Heap Index = {}, Flags = {}", i,
                             MEMORY_PROPS.memoryTypes[i].heapIndex,
                             to_string(static_cast<VkMemoryPropertyFlagBits>(
                                 MEMORY_PROPS.memoryTypes[i].propertyFlags)));
            }

            const auto EXTENSIONS = device.enumerateDeviceExtensionProperties();
            std::println("\n=== Device Extensions ===");
            std::println("Total: {}", EXTENSIONS.size());
            for (const auto &ext : EXTENSIONS)
            {
                std::println("  {} (Version: {})", ext.extensionName, ext.specVersion);
            }

            std::println("pick_physical_device: [end]");
        }
    }; // NOLINTEND

}; // namespace mcs::vulkan::core