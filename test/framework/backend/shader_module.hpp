#pragma once

#include "sType.hpp"
#include "utils/mcs_assert.hpp"
#include "utils/read_file.hpp"
#include "LogicalDevice.hpp"
#include <bit>
#include <string>
#include <utility>

namespace mcs::vulkan::core
{
    struct shader_module
    {
        [[nodiscard]] bool valid() const noexcept
        {
            return shaderModule_ != nullptr;
        }

        constexpr shader_module(const LogicalDevice *device,
                                const VkShaderModuleCreateInfo &createInfo)
            : device_{device},
              shaderModule_{device_->createShaderModule(createInfo, device_->allocator())}
        {
            MCS_ASSERT(valid());
        }
        constexpr shader_module(const LogicalDevice &device,
                                const VkShaderModuleCreateInfo &createInfo)
            : shader_module(&device, createInfo)
        {
        }
        constexpr shader_module(const LogicalDevice &device,
                                const std::vector<char> &code)
            : device_{&device},
              shaderModule_{device_->createShaderModule(
                  {.sType = sType<VkShaderModuleCreateInfo>(),
                   .codeSize = code.size(),
                   .pCode = std::bit_cast<const uint32_t *>(code.data())},
                  device_->allocator())}
        {
            MCS_ASSERT(valid());
        }
        constexpr shader_module(const LogicalDevice &device, const std::string &path)
            : shader_module(device, read_file(path))
        {
        }

        constexpr ~shader_module() noexcept
        {
            destroy();
        }
        constexpr shader_module(shader_module &&o) noexcept
            : device_(std::exchange(o.device_, {})),
              shaderModule_{std::exchange(o.shaderModule_, {})}
        {
        }
        constexpr shader_module &operator=(shader_module &&o) noexcept
        {
            if (&o != this)
            {
                this->destroy();
                device_ = std::exchange(o.device_, {});
                shaderModule_ = std::exchange(o.shaderModule_, {});
            }
            return *this;
        }
        shader_module(const shader_module &) = delete;
        shader_module &operator=(const shader_module &) = delete;

        [[nodiscard]] constexpr auto raw_data() const noexcept // NOLINT
        {
            return shaderModule_;
        }

      private:
        const LogicalDevice *device_;
        VkShaderModule shaderModule_;

        constexpr void destroy() noexcept
        {
            if (device_ != nullptr)
            {
                device_->destroyShaderModule(shaderModule_, device_->allocator());

                shaderModule_ = {};
                device_ = {};
            }
        }
    };

}; // namespace mcs::vulkan::core
