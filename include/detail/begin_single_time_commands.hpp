#pragma once

#include "single_time_command_buffer.hpp"

namespace mcs::vulkan
{
    static constexpr auto begin_single_time_commands(const logical_device &device,
                                                     VkCommandPool commandPool)
        -> single_time_command_buffer
    {
        auto command = single_time_command_buffer{device, commandPool};
        command.begin();
        return command;
    }
}; // namespace mcs::vulkan