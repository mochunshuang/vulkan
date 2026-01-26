#pragma once

#include "to_string.hpp"
#include "bad_result_throw_exception.hpp"
#include <format>

namespace mcs::vulkan
{
    static constexpr auto check_vkresult(VkResult code)
    {
        bad_result_throw_exception(code, to_string(code));
    }
    static constexpr auto check_vkresult(const VkResult &code, const std::string &msg)
    {
        bad_result_throw_exception(code, std::format("{}: {}", msg, to_string(code)));
    }

}; // namespace mcs::vulkan