#pragma once

#include "slot_interface.hpp"
#include "traits_slot.hpp"
#include <functional>

namespace mcs::vulkan::conn
{
    template <typename Rcvr, typename slot_type>
    struct slot_impl : slot_interface
    {
        using traits = traits_slot<slot_type>;
        using ref_args_tuple = traits::ref_args_tuple;
        static constexpr size_t args_size = traits::args_size; // NOLINT

        constexpr void invoke_impl(void *args) noexcept override
        {
            auto &tuple_args = *static_cast<ref_args_tuple *>(args);

            [&]<size_t... I>(std::index_sequence<I...>) noexcept {
                // 1. rcvr_ requires
                if constexpr (requires {
                                  (slot_)(rcvr_, std::move(std::get<I>(tuple_args))...);
                              })
                    (slot_)(rcvr_, std::move(std::get<I>(tuple_args))...);
                else if constexpr (requires {
                                       (slot_)(*rcvr_,
                                               std::move(std::get<I>(tuple_args))...);
                                   })
                    (slot_)(*rcvr_, std::move(std::get<I>(tuple_args))...);
                // 2. member_function
                else if constexpr (std::is_member_function_pointer_v<slot_type> &&
                                   requires {
                                       std::mem_fn(slot_)(
                                           rcvr_, std::move(std::get<I>(tuple_args))...);
                                   })
                    std::mem_fn(slot_)(rcvr_, std::move(std::get<I>(tuple_args))...);
                // 3. no rcvr_ requires
                else if constexpr (requires {
                                       (slot_)(std::move(std::get<I>(tuple_args))...);
                                   })
                    (slot_)(std::move(std::get<I>(tuple_args))...);
                else
                {
                    static_assert(false, "Cannot invoke slot function");
                }
            }(std::make_index_sequence<args_size>{});
        }

        constexpr slot_impl(Rcvr *rcvr, slot_type slot) noexcept
            : rcvr_{rcvr}, slot_{std::move(slot)}
        {
        }

#if defined(_MSC_VER)
#define MY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif defined(__clang__) || defined(__GNUC__)
#define MY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define MY_NO_UNIQUE_ADDRESS // 不支持该属性的编译器
#endif
      private:
        Rcvr *rcvr_;
        MY_NO_UNIQUE_ADDRESS slot_type slot_;
#undef MY_NO_UNIQUE_ADDRESS
    };
}; // namespace mcs::vulkan::conn