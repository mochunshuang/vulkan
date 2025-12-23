#pragma once

#include "./debug_extension.hpp"
#include "instance.hpp"
#include <optional>

namespace mcs::vulkan
{
    struct make_instance
    {
        template <typename Extension>
        auto &addRequiredExtension()
        {
            Extension::addRequiredExtension(requiredInstanceExtensions_);
            return *this;
        }
        template <typename Extension>
        auto &addRequiredLayer()
        {
            Extension::addRequiredLayer(requiredInstanceLayer_);
            return *this;
        }

        template <typename SurfaceExtension>
        auto &enableSurfaceExtension()
        {
            requiredInstanceExtensions_.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
            return addRequiredExtension<SurfaceExtension>();
        }
        auto &enableDebugExtension(VkDebugUtilsMessengerCreateInfoEXT createInfo =
                                       debug_extension::defaultCreateInfo())
        {
            debugCreateInfo_ = createInfo;
            (*debugCreateInfo_).pNext = createInfo.pNext;
            createInfo_.pNext = &(*debugCreateInfo_);
            return addRequiredExtension<debug_extension>()
                .addRequiredLayer<debug_extension>();
        }

        auto &checkExtensionSupport()
        {
            if (not instance::checkExtensionSupport(requiredInstanceExtensions_,
                                                    availableInstanceExtensions_))
                throw std::runtime_error("checkExtensionSupport error.");
            return *this;
        }
        auto &checkLayerSupport()
        {
            if (not instance::checkLayerSupport(requiredInstanceLayer_, availableLayer_))
                throw std::runtime_error("checkLayerSupport error.");
            return *this;
        }

        auto build(const VkApplicationInfo &appInfo, VkInstanceCreateFlags flags = {})
        {
            createInfo_.flags = flags;
            createInfo_.pApplicationInfo = &appInfo;
            createInfo_.enabledLayerCount =
                static_cast<uint32_t>(requiredInstanceLayer_.size());
            createInfo_.ppEnabledLayerNames = requiredInstanceLayer_.data();
            createInfo_.ppEnabledExtensionNames = requiredInstanceExtensions_.data();
            createInfo_.enabledExtensionCount =
                static_cast<uint32_t>(requiredInstanceExtensions_.size());

            instance ist{createInfo_};
            if (debugCreateInfo_)
                ist.createDebugExtension(*debugCreateInfo_);
            return ist;
        }

        static VkApplicationInfo defaultApplicationInfo() noexcept
        {
            return {.sType = vulkan::sType<VkApplicationInfo>(),
                    .pApplicationName = "Hello Triangle",
                    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                    .pEngineName = "No Engine",
                    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                    // apiVersion必须是应用程序设计使用的Vulkan的最高版本
                    .apiVersion = VK_API_VERSION_1_3};
        }

      private:
        std::vector<VkExtensionProperties> availableInstanceExtensions_ =
            instance::availableExtension();
        std::vector<VkLayerProperties> availableLayer_ = instance::availableLayer();
        std::vector<const char *> requiredInstanceExtensions_;
        std::vector<const char *> requiredInstanceLayer_;

        std::optional<VkDebugUtilsMessengerCreateInfoEXT> debugCreateInfo_;
        VkInstanceCreateInfo createInfo_ = {.sType = sType<VkInstanceCreateInfo>()};
    };

}; // namespace mcs::vulkan