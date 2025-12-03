#pragma once

#include <print>

// NOLINTBEGIN
template <typename... Args>
static constexpr auto LOGI(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("INFO: {}", std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
static constexpr auto LOGW(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("WARN: {}", std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
static constexpr auto LOGE(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("ERROR: {}", std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
static constexpr auto LOGD(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("DEBUG: {}", std::format(fmt, std::forward<Args>(args)...));
}
// NOLINTEND