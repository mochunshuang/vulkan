#pragma once

#include <cstdint>
#include <limits>

namespace mcs::vulkan::event
{
    constexpr auto UNDEFINED_double = std::numeric_limits<double>::max(); // NOLINT
    constexpr auto UNDEFINED_int = std::numeric_limits<int>::max();       // NOLINT

    enum class MouseButtons : std::uint8_t
    {
        UNDEFINED,
        MOUSE_BUTTON_1,
        MOUSE_BUTTON_2,
        MOUSE_BUTTON_3,
        MOUSE_BUTTON_4,
        MOUSE_BUTTON_5,
        MOUSE_BUTTON_6,
        MOUSE_BUTTON_7,
        MOUSE_BUTTON_8,
        // 别名化，8bit的映射罢了
        MOUSE_BUTTON_LAST,
        MOUSE_BUTTON_LEFT,
        MOUSE_BUTTON_RIGHT,
        MOUSE_BUTTON_MIDDLE,
    };

    enum class Action : std::uint8_t
    {
        UNDEFINED,
        RELEASE, // 释放
        PRESS,   // 按下
        REPEAT   // 重复（长按）
    };

    enum class Key : std::uint8_t
    {
        UNDEFINED,
        // 特殊键
        UNKNOWN,

        // 可打印字符键 - 空格和标点
        SPACE,
        APOSTROPHE,    // '
        COMMA,         // ,
        MINUS,         // -
        PERIOD,        // .
        SLASH,         // /
        SEMICOLON,     // ;
        EQUAL,         // =
        LEFT_BRACKET,  // [
        BACKSLASH,     // '\'
        RIGHT_BRACKET, // ]
        GRAVE_ACCENT,  // `

        // 数字键
        KEY_0,
        KEY_1,
        KEY_2,
        KEY_3,
        KEY_4,
        KEY_5,
        KEY_6,
        KEY_7,
        KEY_8,
        KEY_9,

        // 字母键
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,

        // 世界键（非美式布局特殊键）
        WORLD_1,
        WORLD_2,

        // 编辑与导航键
        ESCAPE,
        ENTER,
        TAB,
        BACKSPACE,
        INSERT,
        DELETE,
        RIGHT,
        LEFT,
        DOWN,
        UP,
        PAGE_UP,
        PAGE_DOWN,
        HOME,
        END,

        // 锁定键
        CAPS_LOCK,
        SCROLL_LOCK,
        NUM_LOCK,

        // 系统键
        PRINT_SCREEN,
        PAUSE,
        MENU,

        // 功能键 F1-F25
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,
        F25,

        // 小键盘键
        KP_0,
        KP_1,
        KP_2,
        KP_3,
        KP_4,
        KP_5,
        KP_6,
        KP_7,
        KP_8,
        KP_9,
        KP_DECIMAL,  // .
        KP_DIVIDE,   // /
        KP_MULTIPLY, // *
        KP_SUBTRACT, // -
        KP_ADD,      // +
        KP_ENTER,
        KP_EQUAL, // =

        // 修饰键
        LEFT_SHIFT,
        LEFT_CONTROL,
        LEFT_ALT,
        LEFT_SUPER, // Windows键 / Command键
        RIGHT_SHIFT,
        RIGHT_CONTROL,
        RIGHT_ALT,
        RIGHT_SUPER
    };

    /**
     * @brief 修饰键标志位封装类
     *
     * 提供类型安全的修饰键操作，支持位运算和组合检查。
     */
    class ModifierKey
    {
      public:
        // 内部枚举定义
        enum Value : std::uint8_t
        {
            NONE = 0,
            SHIFT = 0x01,
            CONTROL = 0x02,
            ALT = 0x04,
            SUPER = 0x08,
            CAPS_LOCK = 0x10,
            NUM_LOCK = 0x20
        };

      private:
        int value_;

      public:
        // 构造函数
        constexpr ModifierKey() noexcept : value_(NONE) {}
        constexpr explicit ModifierKey(Value v) noexcept
            : value_(static_cast<std::uint8_t>(v))
        {
        }
        constexpr explicit ModifierKey(std::uint8_t v) noexcept : value_(v) {}

        // 获取原始值（用于与GLFW等库交互）
        [[nodiscard]] constexpr std::uint8_t raw_data() const noexcept // NOLINT
        {
            return value_;
        }

        // 检查是否包含特定修饰键
        [[nodiscard]] constexpr bool has(Value key) const noexcept
        {
            return (value_ & static_cast<std::uint8_t>(key)) != 0;
        }

        // 检查是否包含所有指定的修饰键
        [[nodiscard]] constexpr bool hasAll(ModifierKey other) const noexcept
        {
            return (value_ & other.value_) == other.value_;
        }

        // 检查是否包含任意指定的修饰键
        [[nodiscard]] constexpr bool hasAny(ModifierKey other) const noexcept
        {
            return (value_ & other.value_) != 0;
        }

        // 检查是否为空
        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return value_ == NONE;
        }

        // 清除所有修饰键
        constexpr void clear() noexcept
        {
            value_ = NONE;
        }

        // 添加修饰键
        constexpr void add(Value key) noexcept
        {
            value_ |= static_cast<std::uint8_t>(key);
        }

        // 移除修饰键
        constexpr void remove(Value key) noexcept
        {
            value_ &= ~static_cast<std::uint8_t>(key);
        }

        // 运算符重载
        constexpr ModifierKey operator|(Value key) const noexcept
        {
            return ModifierKey(value_ | static_cast<std::uint8_t>(key));
        }

        constexpr ModifierKey operator|(ModifierKey other) const noexcept
        {
            return ModifierKey(value_ | other.value_);
        }

        constexpr ModifierKey &operator|=(Value key) noexcept
        {
            value_ |= static_cast<std::uint8_t>(key);
            return *this;
        }

        constexpr ModifierKey &operator|=(ModifierKey other) noexcept
        {
            value_ |= other.value_;
            return *this;
        }

        constexpr ModifierKey operator&(Value key) const noexcept
        {
            return ModifierKey(value_ & static_cast<std::uint8_t>(key));
        }

        constexpr ModifierKey operator&(ModifierKey other) const noexcept
        {
            return ModifierKey(value_ & other.value_);
        }

        constexpr bool operator==(ModifierKey other) const noexcept
        {
            return value_ == other.value_;
        }

        constexpr bool operator!=(ModifierKey other) const noexcept
        {
            return value_ != other.value_;
        }

        // NOLINTBEGIN
        static constexpr ModifierKey None() noexcept
        {
            return ModifierKey(NONE);
        }
        static constexpr ModifierKey Shift() noexcept
        {
            return ModifierKey(SHIFT);
        }
        static constexpr ModifierKey Control() noexcept
        {
            return ModifierKey(CONTROL);
        }
        static constexpr ModifierKey Alt() noexcept
        {
            return ModifierKey(ALT);
        }
        static constexpr ModifierKey Super() noexcept
        {
            return ModifierKey(SUPER);
        }
        static constexpr ModifierKey CapsLock() noexcept
        {
            return ModifierKey(CAPS_LOCK);
        }
        static constexpr ModifierKey NumLock() noexcept
        {
            return ModifierKey(NUM_LOCK);
        }

        // 常用组合
        static constexpr ModifierKey CtrlShift() noexcept
        {
            return Control() | Shift();
        }
        static constexpr ModifierKey CtrlAlt() noexcept
        {
            return Control() | Alt();
        }
        static constexpr ModifierKey CtrlAltShift() noexcept
        {
            return Control() | Alt() | Shift();
        }
        // NOLINTEND
    };

    // 全局运算符重载
    constexpr ModifierKey operator|(ModifierKey::Value lhs,
                                    ModifierKey::Value rhs) noexcept
    {
        return ModifierKey(lhs) | rhs;
    }

    struct keyboard_event
    {
        Key key{Key::UNDEFINED};
        Action action{Action::UNDEFINED};
        ModifierKey modifier_key{ModifierKey::None()};
        int scancode{UNDEFINED_int};
    };

    struct mousebutton_event
    {
        MouseButtons button{MouseButtons::UNDEFINED};
        Action action{Action::UNDEFINED};
        ModifierKey modifier_key{ModifierKey::None()};
    };

    struct scroll_event
    {
        double xoffset{UNDEFINED_double};
        double yoffset{UNDEFINED_double};
    };

    struct position2d_event
    {
        double xpos{UNDEFINED_double};
        double ypos{UNDEFINED_double};
    };

    struct bool_event
    {
        bool value;
    };

}; // namespace mcs::vulkan::event