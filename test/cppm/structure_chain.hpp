#pragma once

#include <utility>
#include "./vulkan_utils.hpp"

namespace vulkan
{

    template <::std::size_t, typename T>
    struct product_type_element
    {
        T value; // NOLINT
        auto operator==(const product_type_element &) const -> bool = default;
    };

    template <typename, typename...>
    struct product_type_base;

    template <::std::size_t... I, typename... T>
    struct product_type_base<::std::index_sequence<I...>, T...>
        : protected product_type_element<I, T>...
    {
        constexpr product_type_base(T &&...t) noexcept // NOLINT
            : product_type_element<I, T>{std::move(t)}...
        {
            setupNextChain();
        }

        static consteval ::std::size_t size() noexcept
        {
            return sizeof...(T);
        }

        auto &head() noexcept
        {
            return get<0>();
        }

        template <::std::size_t J>
        constexpr auto get() & noexcept -> decltype(auto)
        {
            return this->element_get<J>(*this);
        }
        template <::std::size_t J>
        constexpr auto get() && noexcept -> decltype(auto)
        {
            return this->element_get<J>(::std::move(*this));
        }
        template <::std::size_t J>
        [[nodiscard]] constexpr auto get() const & noexcept -> decltype(auto)
        {
            return this->element_get<J>(*this);
        }

        auto operator==(const product_type_base &) const -> bool = default;

      private:
        template <::std::size_t J, typename S>
        constexpr static auto element_get( // NOLINT
            product_type_element<J, S> &self) noexcept -> S &
        {
            return self.value;
        }
        template <::std::size_t J, typename S>
        constexpr static auto element_get( // NOLINTNEXTLINE // NOLINT
            product_type_element<J, S> &&self) noexcept -> S &&
        {
            return ::std::move(self.value);
        }
        template <::std::size_t J, typename S>
        constexpr static auto element_get( // NOLINT
            const product_type_element<J, S> &self) noexcept -> const S &
        {
            return self.value;
        }

        constexpr void setupNextChain() noexcept
        {
            ((get<I>().sType = sType<decltype(auto(get<I>()))>()), ...);
            if constexpr (sizeof...(T) > 1)
            {
                []<std::size_t... Ix>(auto *self,
                                      std::index_sequence<Ix...>) constexpr noexcept {
                    ((self->template get<Ix>().pNext = &self->template get<Ix + 1>()),
                     ...);
                }(this, std::make_index_sequence<sizeof...(T) - 1>{});
            }
        }
    };

    template <typename... T>
        requires(sizeof...(T) > 0 && (requires(T &t) { t.pNext = nullptr; } && ...))
    struct structure_chain : product_type_base<::std::index_sequence_for<T...>, T...>
    {
        using product_type_base<::std::index_sequence_for<T...>, T...>::product_type_base;
    };
    template <typename... T>
    structure_chain(T &&...) -> structure_chain<::std::decay_t<T>...>;

}; // namespace vulkan