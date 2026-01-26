#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <print>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "Instance.hpp"
#include "LogicalDevice.hpp"
#include "initialization.hpp"
#include "sType.hpp"

#include "PhysicalDevice.hpp"

#include "resources_interface.hpp"
#include "swap_chain_interface.hpp"

namespace mcs::vulkan::core
{
    // NOLINTBEGIN
    consteval uint32_t vkApiVersion(uint32_t variant, uint32_t major, uint32_t minor,
                                    uint32_t patch)
    {
        return VK_MAKE_API_VERSION(variant, major, minor, patch); // NOLINT
    }
    consteval uint32_t vkApiVersion(uint32_t major, uint32_t minor, uint32_t patch)
    {
        return vkApiVersion(0, major, minor, patch); // NOLINT
    }
    consteval uint32_t vkMakeVersion(uint32_t major, uint32_t minor, uint32_t patch)
    {
        return VK_MAKE_VERSION(major, minor, patch); // Vulkan标准宏
    }
    // NOLINTEND

    // NOTE: 目的是包含一切句柄. 多物理设备,多逻辑设备应该是新类
    struct app_context
    {
        /// @brief A debug callback called from Vulkan validation layers.
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                             VkDebugUtilsMessageTypeFlagsEXT /*message_types*/,
                             const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                             void * /*user_data*/) noexcept
        {

            if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
            {
                MCSLOG_ERROR("{} Validation Layer: Error: {}: {}",
                             callback_data->messageIdNumber,
                             callback_data->pMessageIdName, callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
            {
                MCSLOG_WARN("{} Validation Layer: Warning: {}: {}",
                            callback_data->messageIdNumber, callback_data->pMessageIdName,
                            callback_data->pMessage);
            }
            else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) !=
                     0)
            {
                MCSLOG_INFO("{} Validation Layer: Information: {}: {}",
                            callback_data->messageIdNumber, callback_data->pMessageIdName,
                            callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0)
            {
                MCSLOG_DEBUG("{} Validation Layer: Verbose: {}: {}",
                             callback_data->messageIdNumber,
                             callback_data->pMessageIdName, callback_data->pMessage);
            }
            return VK_FALSE;
        }

        static constexpr VkDebugUtilsMessengerCreateInfoEXT defaultCreateInfo() noexcept
        {
            return {.sType = sType<VkDebugUtilsMessengerCreateInfoEXT>(),
                    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                    .pfnUserCallback = &defaultDebugCallback};
        }

        app_context()
            : availableLayer_{instanceLayerProperties(nullptr)},
              availabExtension_{instanceExtensionPropertie(nullptr)}
        {
            std::println("global availableLayer: ");
            for (const auto &l : availableLayer_)
            {
                std::println("  {}", l);
            }
            std::println("global availabExtension: ");
            for (const auto &l : availabExtension_)
            {
                std::println("  {}", l);
            }
        }

        app_context(std::vector<VkLayerProperties> availableLayer,
                    std::vector<VkExtensionProperties> availabExtension) noexcept
            : availableLayer_(std::move(availableLayer)),
              availabExtension_(std::move(availabExtension))
        {
        }
        [[nodiscard]] auto &availableLayer() const noexcept
        {
            return availableLayer_;
        }
        [[nodiscard]] auto &availabExtension() const noexcept
        {
            return availabExtension_;
        }

        ~app_context() noexcept
        {
            VkAllocationCallbacks *pAllocator = nullptr;
            if (allocationCallbacks_.has_value())
                pAllocator = &(*allocationCallbacks_);

            if (instance_)
            {
                for (auto *pipeline : graphicsPipeline_)
                    logicalDevice_.destroyPipeline(pipeline, logicalDevice_.allocator());

                for (auto *layout : pipelineLayout_)
                    logicalDevice_.destroyPipelineLayout(layout,
                                                         logicalDevice_.allocator());
                pipelineLayout_.clear();

                if (logicalDevice_)
                    logicalDevice_.destroyDevice(pAllocator);

                if (surface_ != nullptr)
                    instance_.destroySurfaceKHR(surface_, pAllocator);

                if (debug_ != nullptr)
                    instance_.destroyDebugUtilsMessengerEXT(debug_, pAllocator);

                instance_.destroyInstance(pAllocator);
            }
        }
        app_context(const app_context &) = delete;
        app_context(app_context &&) = delete;
        app_context &operator=(const app_context &) = delete;
        app_context &operator=(app_context &&) = delete;
        auto &clearAvailableLayer() noexcept
        {
            availableLayer_ = {};
            return *this;
        }
        auto &clearAvailableExtension() noexcept
        {
            availabExtension_ = {};
            return *this;
        }

        // c0: 0
        [[nodiscard]] std::optional<VkAllocationCallbacks> &allocationCallbacks() noexcept
        {
            return allocationCallbacks_;
        }
        auto &setAllocationCallbacks(VkAllocationCallbacks allocationCallbacks) noexcept
        {
            allocationCallbacks_ = allocationCallbacks;
            return *this;
        }
        constexpr VkAllocationCallbacks *allocator() noexcept
        {
            VkAllocationCallbacks *pAllocator = nullptr;
            if (allocationCallbacks_.has_value())
                pAllocator = &(*allocationCallbacks_);
            return pAllocator;
        }

        // c0: 1
        auto &setInstance(VkInstance ptr) noexcept
        {
            instance_ = Instance(ptr);
            return *this;
        }
        [[nodiscard]] auto &instance() noexcept
        {
            return instance_;
        }

        // c0: 2
        auto &setDebug(VkDebugUtilsMessengerCreateInfoEXT debug = defaultCreateInfo())
        {
            debug_ = instance_.createDebugUtilsMessengerEXT(&debug, allocator());
            return *this;
        }

        // c0: 3
        [[nodiscard]] VkSurfaceKHR surface() const noexcept
        {
            return surface_;
        }
        auto &setSurface(const VkSurfaceKHR &surface) noexcept
        {
            surface_ = surface;
            return *this;
        }

        // c0: 4
        [[nodiscard]] auto &physicalDevice() noexcept
        {
            return physicalDevices_;
        }
        auto &replacePhysicalDevice(size_t /*idx*/, PhysicalDevice &&device) noexcept
        {
            physicalDevices_ = std::move(device); // NOLINT
            return *this;
        }
        auto &setPhysicalDevice(std::pair<size_t, PhysicalDevice> pair) noexcept
        {
            return replacePhysicalDevice(pair.first, std::move(pair.second)); // NOLINT
        }

        auto &replacePhysicalDevice(std::pair<size_t, PhysicalDevice> pair) noexcept
        {
            return replacePhysicalDevice(pair.first, std::move(pair.second)); // NOLINT
        }

        // c0: 5
        [[nodiscard]] auto &logicalDevice() const noexcept
        {
            return logicalDevice_;
        }
        auto &setLogicalDevice(LogicalDevice &&logicalDevice) noexcept
        {
            logicalDevice_ = std::move(logicalDevice); // NOLINT
            return *this;
        }

        // c0: 6 swap_chain

        // c0: 7: pipelineLayout
        size_t addPipelineLayout(VkPipelineLayout layout)
        {
            pipelineLayout_.emplace_back(layout);
            return pipelineLayout_.size() - 1;
        }
        VkPipelineLayout getPipelineLayout(size_t idx) noexcept
        {
            return pipelineLayout_[idx];
        }
        // c0:8: graphicsPipeline
        size_t addGraphicsPipeline(VkPipeline pipeline)
        {
            graphicsPipeline_.emplace_back(pipeline);
            return graphicsPipeline_.size() - 1;
        }
        VkPipeline getGraphicsPipeline(size_t idx) noexcept
        {
            return graphicsPipeline_[idx];
        }

        // c0:
        size_t addResources(std::unique_ptr<resources_interface> res)
        {
            resources_.emplace_back(std::move(res));
            return resources_.size() - 1;
        }
        resources_interface &getResource(size_t idx) noexcept
        {
            return *resources_[idx];
        }

      private:
        std::vector<VkLayerProperties> availableLayer_;
        std::vector<VkExtensionProperties> availabExtension_;

        // 如果 pAllocator 不为 NULL，则 pAllocator 必须是指向有效
        // VkAllocationCallbacks 结构的有效指针
        std::optional<VkAllocationCallbacks> allocationCallbacks_;

        // Vulkan instance, stores all per-application states
        Instance instance_;
        VkDebugUtilsMessengerEXT debug_{nullptr};
        VkSurfaceKHR surface_{};
        PhysicalDevice physicalDevices_;
        LogicalDevice logicalDevice_;

        std::vector<VkPipelineLayout> pipelineLayout_;

        std::vector<VkPipeline> graphicsPipeline_;

        std::vector<std::unique_ptr<resources_interface>> resources_;
    };

}; // namespace mcs::vulkan::core
