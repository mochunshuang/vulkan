#pragma once

#include <set>

#include "app_context.hpp"
#include "sType.hpp"

#include "utils/bad_result_throw_exception.hpp"

#include "check/check_extension_support.hpp"
#include "check/check_layer_support.hpp"

namespace std
{
    template <>
    struct formatter<VkApplicationInfo>
    {
        static constexpr auto parse(std::format_parse_context &ctx) noexcept
        {
            return ctx.begin();
        }

        static auto format(const VkApplicationInfo &appInfo, std::format_context &ctx)
        {
            auto safe_str = [](const char *s) noexcept {
                return s ? s : "(null)";
            };
            std::string api_str =
                std::format("{}.{}.{}", VK_VERSION_MAJOR(appInfo.apiVersion),
                            VK_VERSION_MINOR(appInfo.apiVersion),
                            VK_VERSION_PATCH(appInfo.apiVersion));

            return std::format_to(
                ctx.out(), "App:{} v{}.{}.{} | Engine:{} v{}.{}.{} | Vulkan:{}",
                safe_str(appInfo.pApplicationName),
                VK_VERSION_MAJOR(appInfo.applicationVersion),
                VK_VERSION_MINOR(appInfo.applicationVersion),
                VK_VERSION_PATCH(appInfo.applicationVersion),
                safe_str(appInfo.pEngineName), VK_VERSION_MAJOR(appInfo.engineVersion),
                VK_VERSION_MINOR(appInfo.engineVersion),
                VK_VERSION_PATCH(appInfo.engineVersion), api_str);
        }
    };
}; // namespace std

namespace mcs::vulkan::core
{
    struct create_instance
    {
        VkInstance create()
        {
            print();

            VkInstance instance; // NOLINT

            std::vector<const char *> enabledLayer = enabledLayers();
            std::vector<const char *> enabledExtension = enabledExtensions();
            check::check_extension_support(enabledExtension, ctx_->availabExtension());
            check::check_layer_support(enabledLayer, ctx_->availableLayer());

            // apply
            createInfo_.flags = flags_;
            createInfo_.pApplicationInfo = &applicationInfo_;
            createInfo_.enabledLayerCount = static_cast<uint32_t>(enabledLayer.size());
            createInfo_.ppEnabledLayerNames = enabledLayer.data();
            createInfo_.ppEnabledExtensionNames = enabledExtension.data();
            createInfo_.enabledExtensionCount =
                static_cast<uint32_t>(enabledExtension.size());

            bad_result_throw_exception(
                vkCreateInstance(&createInfo_, ctx_->allocator(), &instance),
                "vkCreateInstance error.");
            return instance;
        }

        auto &setFlags(const VkInstanceCreateFlags &flags) noexcept
        {
            flags_ = flags;
            return *this;
        }

        [[nodiscard]] VkApplicationInfo applicationInfo() const noexcept
        {
            return applicationInfo_;
        }
        auto &setApplicationInfo(const VkApplicationInfo &applicationInfo)
        {
            applicationInfo_ = applicationInfo;
            return *this;
        }

        [[nodiscard]] std::vector<const char *> enabledLayers() const
        {
            std::vector<const char *> enabledLayer;
            enabledLayer.append_range(enabledLayer_);
            return enabledLayer;
        }
        auto &setEnabledLayers(const std::set<const char *> &enabledLayer)
        {
            enabledLayer_ = enabledLayer;
            return *this;
        }

        [[nodiscard]] std::vector<const char *> enabledExtensions() const
        {
            std::vector<const char *> enabledExtension;
            enabledExtension.append_range(enabledExtension_);
            return enabledExtension;
        }
        auto &setEnabledExtensions(const std::set<const char *> &enabledExtension)
        {
            enabledExtension_ = enabledExtension;
            return *this;
        }

        void print()
        {
            std::println("\nVkInstanceCreateInfo: [begin]");
            std::println("flags: {}, app: {}", flags_, applicationInfo_);

            std::println("enableDebug: {}",
                         enabledExtension_.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
            std::println("enableSurface: {}",
                         enabledExtension_.contains(VK_KHR_SURFACE_EXTENSION_NAME));

            std::println("enabledLayer:");
            for (const auto &l : enabledLayer_)
            {
                std::println("  {}", l);
            }

            std::println("enabledExtension:");
            for (const auto &l : enabledExtension_)
            {
                std::println("  {}", l);
            }

            std::println("VkInstanceCreateInfo: [end]\n");
        }

        //-------------------------------

        auto &addEnableExtension(const char *const EXTENSION)
        {
            enabledExtension_.insert(EXTENSION);
            return *this;
        }
        auto &addEnableLayer(const char *const LAYER)
        {
            enabledLayer_.insert(LAYER);
            return *this;
        }

        // 特殊
        auto &enableDebug()
        {
            addEnableExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            addEnableLayer("VK_LAYER_KHRONOS_validation");
            return *this;
        }
        auto &enableValidation()
        {
            return enableDebug();
        }
        template <typename surface>
        auto &enableSurface()
        {
            addEnableExtension(VK_KHR_SURFACE_EXTENSION_NAME);
            enabledExtension_.insert_range(surface::requiredVulkanInstanceExtensions());
            return *this;
        }

        create_instance() = delete;
        explicit create_instance(app_context &context) noexcept : ctx_{&context} {}

        VkInstanceCreateInfo &createInfo()
        {
            return createInfo_;
        }

      private:
        app_context *ctx_;
        /*
        typedef struct VkInstanceCreateInfo {
            VkStructureType             sType;
            const void*                 pNext;
            VkInstanceCreateFlags       flags;
            const VkApplicationInfo*    pApplicationInfo;
            uint32_t                    enabledLayerCount;
            const char* const*          ppEnabledLayerNames;
            uint32_t                    enabledExtensionCount;
            const char* const*          ppEnabledExtensionNames;
        } VkInstanceCreateInfo;
        */
        VkInstanceCreateInfo createInfo_{.sType = sType<VkInstanceCreateInfo>()};

        // impl
        VkInstanceCreateFlags flags_{};
        VkApplicationInfo applicationInfo_{.sType = sType<VkApplicationInfo>()};
        std::set<const char *> enabledLayer_;
        std::set<const char *> enabledExtension_;
    };
}; // namespace mcs::vulkan::core
