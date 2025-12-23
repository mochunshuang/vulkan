#pragma once

#include <unordered_set>

namespace mcs::vulkan::event
{
    template <typename T>
    struct event_dispatcher
    {
        using event_type = T;
        using callback_type = void(event_type event) noexcept;

        constexpr void distribute(event_type event) noexcept
        {
            for (auto *callback : callbacks_)
                (callback)(event);
        }
        constexpr void subscribe(callback_type *callback)
        {
            callbacks_.emplace(callback);
        }
        constexpr void unsubscribe(callback_type *callback)
        {
            callbacks_.erase(callback);
        }

        constexpr static auto &instance() noexcept
        {
            static event_dispatcher instance;
            return instance;
        }

      private:
        std::unordered_set<callback_type *> callbacks_;
        event_dispatcher() = default;
    };

}; // namespace mcs::vulkan::event