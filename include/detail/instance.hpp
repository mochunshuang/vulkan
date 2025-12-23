#pragma once

#include "./vk_api/vk_instance_api.hpp"

#include "get_fun.hpp"
#include "utils/mcs_assert.hpp"

#include <utility>

namespace mcs::vulkan
{
    struct instance : vk_api::vk_instance_api
    {
        using value_type = VkInstance;

        constexpr void createDebugExtension(
            const VkDebugUtilsMessengerCreateInfoEXT &createInfo)
        {
            MCS_ASSERT(not isEnableDebugExtension());
            MCS_ASSERT(valid());
            PFN_vkCreateDebugUtilsMessengerEXT fun =
                getFunPtr<PFN_vkCreateDebugUtilsMessengerEXT>(instance_);
            utils::check_vk_result(fun(instance_, &createInfo, nullptr, &debugExt_));
        }

        constexpr instance() noexcept = default;
        constexpr explicit instance(const VkInstanceCreateInfo &createInfo)
        {
            utils::check_vk_result(::vkCreateInstance(&createInfo, nullptr, &instance_));
        }
        constexpr ~instance() noexcept
        {
            destroy();
        }
        constexpr instance(instance &&other) noexcept
            : instance_{std::exchange(other.instance_, {})},
              debugExt_{std::exchange(other.debugExt_, {})} {

              };
        constexpr instance &operator=(instance &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                this->instance_ = std::exchange(other.instance_, {});
                this->debugExt_ = std::exchange(other.debugExt_, {});
            }
            return *this;
        };

        instance(const instance &) = delete;
        instance &operator=(const instance &) = delete;

        // help fun
        constexpr value_type &ref_data() & noexcept // NOLINT
        {
            return instance_;
        }
        [[nodiscard]] constexpr const value_type &ref_data() const & noexcept // NOLINT
        {
            return instance_;
        }
        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return instance_ != nullptr;
        }
        constexpr explicit operator bool() const noexcept
        {
            return valid();
        }
        [[nodiscard]] bool isEnableDebugExtension() const noexcept
        {
            return debugExt_ != nullptr;
        }

      private:
        value_type instance_ = nullptr;
        VkDebugUtilsMessengerEXT debugExt_ = nullptr;

        constexpr void destroy() noexcept
        {
            if (instance_ != nullptr)
            {
                destroyDebugExtension();
                ::vkDestroyInstance(instance_, nullptr);
                instance_ = nullptr;
            }
        }

        constexpr void destroyDebugExtension() noexcept
        {
            if (debugExt_ != nullptr)
            {
                PFN_vkDestroyDebugUtilsMessengerEXT fun =
                    getFunPtr<PFN_vkDestroyDebugUtilsMessengerEXT>(instance_);
                fun(instance_, debugExt_, nullptr);
                debugExt_ = nullptr;
            }
        }
    };

}; // namespace mcs::vulkan