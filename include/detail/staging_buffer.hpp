#pragma once

#include "buffer_base.hpp"
#include <cstddef>

namespace mcs::vulkan
{

    struct staging_buffer : buffer_base
    {
        constexpr void mapAndUnmapMempry(const void *src, size_t size) const noexcept
        {
            void *data; // NOLINT
            buffer_base::device()->mapMempry(bufferMemory(), 0, size, 0, &data);
            ::memcpy(data, src, size);
            buffer_base::device()->unmapMemory(bufferMemory());
        }
    };

}; // namespace mcs::vulkan