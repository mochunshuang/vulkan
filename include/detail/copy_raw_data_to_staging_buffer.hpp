#pragma once

#include "raw_stbi_image.hpp"
#include "staging_buffer.hpp"
#include <cstddef>

namespace mcs::vulkan
{
    constexpr static void copy_raw_data_to_staging_buffer(
        void *src, size_t size, const staging_buffer &staging_buffer)
    {
        staging_buffer.mapAndUnmapMemory(src, size);
    }

    constexpr static void copy_raw_data_to_staging_buffer(
        const raw_stbi_image &raw_data, const staging_buffer &staging_buffer)
    {
        copy_raw_data_to_staging_buffer(raw_data.data(), raw_data.imageSize(),
                                        staging_buffer);
    }

}; // namespace mcs::vulkan