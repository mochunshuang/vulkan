#pragma once

#include "initialization.hpp"

namespace mcs::vulkan::core
{
    template <typename Fn>
    static constexpr auto proc_addr(auto distributor, const char *pName) noexcept
    {
        return std::bit_cast<Fn>(getFuncPtr(distributor, pName));
    }
}; // namespace mcs::vulkan::core