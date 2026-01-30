#pragma once

#include <type_traits>
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan::core
{
    template <typename FlagBits>
        requires(std::is_enum_v<FlagBits>)
    class VkFlagsWrapper
    {
        using underlying_type = std::underlying_type_t<FlagBits>;

        using value_type = ::VkFlags;
        value_type value_;

      public:
        // constexpr value_type &operator*() noexcept
        // {
        //     return value_;
        // }
        // constexpr const value_type &operator*() const noexcept
        // {
        //     return value_;
        // }

        // 转换为底层类型 比上面可读性好 // NOLINTNEXTLINE
        constexpr operator value_type() const noexcept
        {
            return value_;
        }

        // 从枚举值构造 // NOLINTNEXTLINE  允许隐式转化
        constexpr VkFlagsWrapper(FlagBits flag) noexcept
            : value_(static_cast<underlying_type>(flag))
        {
        }
        constexpr VkFlagsWrapper() noexcept : VkFlagsWrapper(FlagBits{}) {}
        // 从底层值构造 // NOLINTNEXTLINE  允许隐式转化.
        constexpr VkFlagsWrapper(underlying_type val) noexcept : value_(val) {}

        VkFlagsWrapper(VkFlagsWrapper &&) = default;
        VkFlagsWrapper &operator=(const VkFlagsWrapper &) = default;
        VkFlagsWrapper &operator=(VkFlagsWrapper &&) = default;
        ~VkFlagsWrapper() = default;

        // 复制构造
        constexpr VkFlagsWrapper(const VkFlagsWrapper &) = default;

        // 转换为包装器本身（为了重载解析）
        constexpr VkFlagsWrapper operator~() const noexcept
        {
            return VkFlagsWrapper(~value_);
        }

        // 位运算
        constexpr VkFlagsWrapper operator|(FlagBits flag) const noexcept
        {
            return VkFlagsWrapper(value_ | static_cast<underlying_type>(flag));
        }

        constexpr VkFlagsWrapper operator|(VkFlagsWrapper other) const noexcept
        {
            return VkFlagsWrapper(value_ | other.value_);
        }

        constexpr VkFlagsWrapper operator&(FlagBits flag) const noexcept
        {
            return VkFlagsWrapper(value_ & static_cast<underlying_type>(flag));
        }

        constexpr VkFlagsWrapper operator&(VkFlagsWrapper other) const noexcept
        {
            return VkFlagsWrapper(value_ & other.value_);
        }

        constexpr VkFlagsWrapper operator^(FlagBits flag) const noexcept
        {
            return VkFlagsWrapper(value_ ^ static_cast<underlying_type>(flag));
        }

        constexpr VkFlagsWrapper operator^(VkFlagsWrapper other) const noexcept
        {
            return VkFlagsWrapper(value_ ^ other.value_);
        }

        // 复合赋值
        constexpr VkFlagsWrapper &operator|=(FlagBits flag) noexcept
        {
            value_ |= static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr VkFlagsWrapper &operator|=(VkFlagsWrapper other) noexcept
        {
            value_ |= other.value_;
            return *this;
        }

        constexpr VkFlagsWrapper &operator&=(FlagBits flag) noexcept
        {
            value_ &= static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr VkFlagsWrapper &operator&=(VkFlagsWrapper other) noexcept
        {
            value_ &= other.value_;
            return *this;
        }

        constexpr VkFlagsWrapper &operator^=(FlagBits flag) noexcept
        {
            value_ ^= static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr VkFlagsWrapper &operator^=(VkFlagsWrapper other) noexcept
        {
            value_ ^= other.value_;
            return *this;
        }

        // 比较运算符
        friend constexpr bool operator==(VkFlagsWrapper a,
                                         VkFlagsWrapper b) noexcept = default;
    };
}; // namespace mcs::vulkan::core