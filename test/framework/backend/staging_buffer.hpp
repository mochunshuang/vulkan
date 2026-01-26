

#include "buffer_base.hpp"
#include <cstddef>

namespace mcs::vulkan::core
{

    struct staging_buffer : buffer_base
    {
        constexpr void copyToBuffer(const void *src, size_t size) const noexcept
        {
            void *data; // NOLINT
            buffer_base::device()->mapMemory(bufferMemory(), 0, size, 0, &data);
            ::memcpy(data, src, size);
            buffer_base::device()->unmapMemory(bufferMemory());
        }
    };

}; // namespace mcs::vulkan::core
