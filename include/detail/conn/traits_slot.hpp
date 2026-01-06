#pragma once

#include "../concepts/no_cvref.hpp"
#include "../concepts/raw_lref.hpp"
#include "../concepts/raw_pointer.hpp"

#include <tuple>

namespace mcs::vulkan::conn
{
    namespace detail
    {
        using concepts::no_cvref;
        using concepts::raw_lref;
        using concepts::raw_pointer;

        // NOLINTBEGIN
        template <typename T>
        struct slot_function;

        //---------------- function ptr -----------------------
        template <typename Sloter, no_cvref... Args>
            requires(raw_pointer<Sloter> || raw_lref<Sloter>)
        struct slot_function<void (*)(Sloter, Args...) noexcept>
        {
            using sloter_type = Sloter;
            using args_tuple = std::tuple<Args...>;
            using ref_args_tuple = std::tuple<Args &...>;
            static constexpr size_t args_size = sizeof...(Args);
        };
        template <typename Sloter>
            requires(raw_pointer<Sloter> || raw_lref<Sloter>)
        struct slot_function<void (*)(Sloter) noexcept>
        {
            using sloter_type = Sloter;
            using args_tuple = std::tuple<>;
            using ref_args_tuple = std::tuple<>;
            static constexpr size_t args_size = 0;
        };

        //---------------- lambda type -----------------------
        template <typename lambda, typename Sloter, no_cvref... Args>
            requires(raw_pointer<Sloter> || raw_lref<Sloter>)
        struct slot_function<void (lambda ::*)(Sloter, Args...) noexcept>
        {
            using sloter_type = Sloter;
            using args_tuple = std::tuple<Args...>;
            using ref_args_tuple = std::tuple<Args &...>;
            static constexpr size_t args_size = sizeof...(Args);
        };
        template <typename lambda, typename Sloter, no_cvref... Args>
            requires(raw_pointer<Sloter> || raw_lref<Sloter>)
        struct slot_function<void (lambda ::*)(Sloter, Args...) const noexcept>
        {
            using sloter_type = Sloter;
            using args_tuple = std::tuple<Args...>;
            using ref_args_tuple = std::tuple<Args &...>;
            static constexpr size_t args_size = sizeof...(Args);
        };
        template <typename lambda, typename Sloter>
            requires(raw_pointer<Sloter> || raw_lref<Sloter>)
        struct slot_function<void (lambda ::*)(Sloter) noexcept>
        {
            using sloter_type = Sloter;
            using args_tuple = std::tuple<>;
            using ref_args_tuple = std::tuple<>;
            static constexpr size_t args_size = 0;
        };

        template <typename lambda, typename Sloter>
            requires(raw_pointer<Sloter> || raw_lref<Sloter>)
        struct slot_function<void (lambda ::*)(Sloter) const noexcept>
        {
            using sloter_type = Sloter;
            using args_tuple = std::tuple<>;
            using ref_args_tuple = std::tuple<>;
            static constexpr size_t args_size = 0;
        };

        // class member or lambda
        template <typename Class, typename... Args>
        struct slot_function<void (Class ::*)(Args...) const noexcept>
        {
            using sloter_type = Class;
            using args_tuple = std::tuple<Args...>;
            using ref_args_tuple = std::tuple<Args...>;
            static constexpr size_t args_size = sizeof...(Args);
        };
        template <typename Class, typename... Args>
        struct slot_function<void (Class ::*)(Args...) noexcept>
        {
            using sloter_type = Class;
            using args_tuple = std::tuple<Args...>;
            using ref_args_tuple = std::tuple<Args...>;
            static constexpr size_t args_size = sizeof...(Args);
        };

        // NOLINTEND
    }; // namespace detail

    template <typename T>
        requires(
            (std::is_object_v<T> &&
             requires() {
                 typename detail::slot_function<decltype(&T::operator())>::sloter_type;
             }) ||
            ((std::is_pointer_v<T> || std::is_member_function_pointer_v<T>) &&
             requires() { typename detail::slot_function<T>::sloter_type; }))
    struct traits_slot
    {
        using function_type = decltype([]() consteval {
            if constexpr (std::is_object_v<T> && requires() { &T::operator(); })
                return static_cast<decltype(&T::operator())>(nullptr);
            else
                return static_cast<T>(nullptr);
        }());
        // NOLINTBEGIN
        static constexpr bool is_lambda = std::is_object_v<T>;
        using traits = detail::slot_function<function_type>;
        using sloter_type = traits::sloter_type;
        using args_tuple = typename traits::args_tuple;
        using ref_args_tuple = typename traits::ref_args_tuple;
        static constexpr size_t args_size = traits::args_size;
        static constexpr bool sloter_type_is_pointer = std::is_pointer_v<sloter_type>;
        // NOLINTEND
    };

}; // namespace mcs::vulkan::conn