#pragma once

#include "instance.hpp"
#include "surface_extension.hpp"
#include "physical_device.hpp"
#include "logical_device.hpp"

#include "./utils/vk_exception.hpp"
#include <utility>
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan
{
    struct context_base
    {
      public:
        auto &createInstance(instance &&instance)
        {
            instance_ = std::move(instance);
            if (not instance_.valid())
                throw utils::make_vk_exception("instance is not valid.");

            return *this;
        }

        template <typename Surface>
        auto &createSurface(const Surface &surface)
        {
            surface_ = surface.createVkSurfaceKHR(instance_.ref_data());
            if (surface_ == nullptr)
                throw utils::make_vk_exception("instance is not valid.");
            return *this;
        }

        auto &createPhysicalDevice(const physical_device &PhysicalDevice)
        {
            physicalDevice_ = PhysicalDevice;
            if (not physicalDevice_.valid())
                throw utils::make_vk_exception("physical_device is not valid.");
            return *this;
        }

        auto &createLogicalDevice(logical_device &&logicalDevice)
        {
            logicalDevice_ = std::move(logicalDevice);
            if (not logicalDevice_.valid())
                throw utils::make_vk_exception("logical_device is not valid.");
            return *this;
        }

        auto &createDefaultQueue(const VkQueue &defalutQueue) noexcept
        {
            defaultQueue_ = defalutQueue;
            return *this;
        }
        auto &createDefalutCommandPool(VkCommandPool commandPool) noexcept
        {
            commandPool_ = commandPool;
            return *this;
        }

        context_base() = default;
        context_base(const context_base &) = delete;
        context_base(context_base &&) = delete;
        context_base &operator=(const context_base &) = delete;
        context_base &operator=(context_base &&) = delete;

        ~context_base() noexcept
        {
            destroy();
        }

        [[nodiscard]] VkInstance raw_instance() const noexcept // NOLINT
        {
            return instance_.ref_data();
        }
        [[nodiscard]] auto &ref_instance() const & noexcept // NOLINT
        {
            return instance_;
        }

        [[nodiscard]] VkSurfaceKHR surface() const noexcept
        {
            return surface_;
        }

        [[nodiscard]] VkPhysicalDevice raw_physical_device() const noexcept // NOLINT
        {
            return physicalDevice_.raw_data();
        }
        [[nodiscard]] auto &ref_physical_device() const & noexcept // NOLINT
        {
            return physicalDevice_;
        }

        [[nodiscard]] VkDevice raw_logical_device() const noexcept // NOLINT
        {
            return logicalDevice_.raw_data();
        }
        [[nodiscard]] auto &ref_logical_device() const & noexcept // NOLINT
        {
            return logicalDevice_;
        }

        [[nodiscard]] VkQueue defaultQueue() const noexcept
        {
            return defaultQueue_;
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return instance_.valid() && physicalDevice_.valid() &&
                   logicalDevice_.valid() && defaultQueue_ != nullptr;
        }

        [[nodiscard]] VkCommandPool defalutCommandPool() const noexcept
        {
            return commandPool_;
        }

        [[nodiscard]] VkSurfaceCapabilitiesKHR getSurfaceCapabilitiesKHR() const noexcept
        {
            return ref_physical_device().getSurfaceCapabilitiesKHR(surface());
        }
        [[nodiscard]] std::vector<VkSurfaceFormatKHR> getSurfaceFormatsKHR() const
        {
            return ref_physical_device().getSurfaceFormatsKHR(surface());
        }
        [[nodiscard]] VkSampleCountFlagBits getMaxUsableSampleCount() const noexcept
        {
            return ref_physical_device().getMaxUsableSampleCount();
        }

        [[nodiscard]] std::vector<VkPresentModeKHR> getSurfacePresentModesKHR() const
        {
            return ref_physical_device().getSurfacePresentModesKHR(surface());
        }

      private:
        instance instance_;
        VkSurfaceKHR surface_ = nullptr;
        physical_device physicalDevice_;
        logical_device logicalDevice_;
        VkQueue defaultQueue_ = nullptr;
        VkCommandPool commandPool_ = nullptr;

        constexpr void destroy() noexcept
        {
            if (commandPool_ != nullptr)
            {
                logicalDevice_.destroyCommandPool(commandPool_);
                commandPool_ = nullptr;
            }

            if (instance_.valid() && surface_ != nullptr)
            {
                surface_extension::destroy(instance_.ref_data(), surface_);
                surface_ = nullptr;
            }
        }
    };

}; // namespace mcs::vulkan