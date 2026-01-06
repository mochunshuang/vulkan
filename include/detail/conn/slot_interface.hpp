#pragma once

#include <utility>
#include <tuple>

namespace mcs::vulkan::conn
{
    struct slot_interface
    {
        slot_interface() = default;
        slot_interface(const slot_interface &) = default;
        slot_interface(slot_interface &&) = default;
        slot_interface &operator=(const slot_interface &) = default;
        slot_interface &operator=(slot_interface &&) = default;

        constexpr virtual void invoke_impl(void *args) noexcept = 0; // NOLINT
        constexpr virtual ~slot_interface() noexcept = default;

        template <typename... Args>
        constexpr void invoke(Args &&...args) & noexcept
        {
            auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
            invoke_impl(&args_tuple);
        }
    };
}; // namespace mcs::vulkan::conn