#pragma once
#include "./event_type.hpp"
#include "./event_dispatcher.hpp"

namespace mcs::vulkan::event
{
    struct cursor_enter_event_dispatcher : event_dispatcher<bool_event>
    {
    };
}; // namespace mcs::vulkan::event