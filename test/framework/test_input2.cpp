#include "./head.hpp"
#include <cstddef>
#include <iostream>
#include <chrono>
#include <print>
#include <thread>
#include <tuple>
#include <utility>
#include <array>

using surface = mcs::vulkan::wsi::glfw::Window;
using glfw_input = mcs::vulkan::input::glfw_input;
using Key = mcs::vulkan::event::Key;
using ModifierKey = mcs::vulkan::event::ModifierKey;
using MouseButtons = mcs::vulkan::event::MouseButtons;
using Action = mcs::vulkan::event::Action;

// NOLINTBEGIN
struct status_mousebutton
{
    using mousebutton_event = mcs::vulkan::event::mousebutton_event;
    // 赋值操作符
    status_mousebutton &operator=(const mousebutton_event &evt) noexcept
    {
        if (evt == mousebutton_event{})
            return *this;
        value_ = evt;
        std::cout << "value_: " << value_ << '\n';
        return *this;
    }
    [[nodiscard]] bool press() const noexcept
    {
        return value_.press();
    }
    [[nodiscard]] bool release() const noexcept
    {
        return value_.release();
    }
    [[nodiscard]] bool repeat() const noexcept
    {
        return value_.repeat();
    }
    [[nodiscard]] bool isModifier(ModifierKey status) const noexcept
    {
        return value_.isModifier(status);
    }
    friend constexpr bool operator==(const status_mousebutton &a,
                                     const status_mousebutton &b) noexcept = default;

    friend constexpr bool operator==(const status_mousebutton a,
                                     const mousebutton_event &b) noexcept
    {
        return a.value_ == b;
    }
    friend constexpr bool operator==(const mousebutton_event &a,
                                     const status_mousebutton &b) noexcept
    {
        return b == a;
    }

    mousebutton_event value_;
};

template <typename F>
struct event_filter
{
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;

    bool pass{false};
    F fn;
    event_filter() = delete;
    template <typename Func>
        requires std::is_invocable_r_v<bool, Func, const glfw_input &>
    constexpr explicit event_filter(Func &&func) noexcept : fn(std::forward<Func>(func))
    {
    }
    constexpr bool filter(const glfw_input &input) noexcept
    {
        static_assert(noexcept(fn(input)), "filter function must be noexcept");
        if (not pass)
            pass = fn(input);
        return pass;
    }
    constexpr bool operator()(const glfw_input &input) noexcept
    {
        return filter(input);
    }
};
template <typename Func>
event_filter(Func) -> event_filter<Func>;

template <size_t availd_interval_ms, typename... Events>
    requires(sizeof...(Events) > 0)
struct event_window
{
  public:
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    using duration = std::chrono::milliseconds;
    static constexpr auto INVALID = (time_point::max)();

    static constexpr size_t SIZE = sizeof...(Events);

    enum class status
    {
        no_pass,
        pass,
        pass_but_no_consume
    };

    // 构造函数
    template <typename... Filters>
        requires(sizeof...(Filters) == sizeof...(Events))
    constexpr explicit event_window(Filters &&...filters)
        : filter_event{std::forward<Filters>(filters)...},
          start_{std::chrono::steady_clock::now()}, status_{status::no_pass}
    {
    }

    auto filter(const glfw_input &input, time_point cur)
    {
        // NOTE: 必须第一个状态走完开始计时
        std::get<0>(filter_event)(input);
        if (start_ == INVALID)
        {
        }
    }

    void update(const glfw_input &input)
    {
        update(input, std::chrono::steady_clock::now());
    }

    void reset_by_timeout() noexcept
    {
        reset_impl(std::index_sequence_for<Events...>{});
    }

    bool all_pass() const noexcept
    {
        return all_pass_impl(std::index_sequence_for<Events...>{});
    }

    void consume() noexcept
    {
        if (status_ == status::pass)
        {
            status_ = status::no_pass;
            reset_by_timeout();
        }
    }

    bool is_pass() const noexcept
    {
        return status_ == status::pass;
    }

    bool is_pass_but_no_consume() const noexcept
    {
        return status_ == status::pass_but_no_consume;
    }

    status get_status() const noexcept
    {
        return status_;
    }

    // 获取剩余时间（毫秒）
    [[nodiscard]] long long remaining_ms(time_point cur) const noexcept
    {
        auto elapsed = std::chrono::duration_cast<duration>(cur - start_);
        if (elapsed >= duration(availd_interval_ms))
            return 0;
        return (availd_interval_ms - elapsed.count());
    }

    [[nodiscard]] long long remaining_ms() const noexcept
    {
        return remaining_ms(std::chrono::steady_clock::now());
    }

    // 重置状态（手动重置）
    void reset() noexcept
    {
        start_ = std::chrono::steady_clock::now();
        status_ = status::no_pass;
        reset_by_timeout();
    }

    // 获取特定过滤器的状态
    template <size_t Index>
    [[nodiscard]] bool filter_pass() const noexcept
    {
        static_assert(Index < SIZE, "Index out of range");
        return std::get<Index>(filter_event).pass;
    }

    // 获取所有过滤器的状态
    std::array<bool, SIZE> filter_passes() const noexcept
    {
        return filter_passes_impl(std::index_sequence_for<Events...>{});
    }

  private:
    template <size_t... I>
    void update_impl(const glfw_input &input, std::index_sequence<I...>) noexcept
    {
        // 只有未通过的过滤器才需要重新过滤
        ((std::get<I>(filter_event).pass ? void(0)
                                         : std::get<I>(filter_event).filter(input)),
         ...);
    }

    template <size_t... I>
    void reset_impl(std::index_sequence<I...>) noexcept
    {
        ((std::get<I>(filter_event).pass = false), ...);
    }

    template <size_t... I>
    [[nodiscard]] bool all_pass_impl(std::index_sequence<I...>) const noexcept
    {
        return (std::get<I>(filter_event).pass && ...);
    }

    template <size_t... I>
    std::array<bool, SIZE> filter_passes_impl(std::index_sequence<I...>) const noexcept
    {
        return {std::get<I>(filter_event).pass...};
    }

    std::tuple<Events...> filter_event{};
    time_point start_{};
    status status_{status::no_pass};
};

// 辅助函数，简化创建
template <size_t interval_ms, typename... Filters>
auto make_event_window(Filters &&...filters)
{
    return event_window<interval_ms, Filters...>(std::forward<Filters>(filters)...);
}

int main()
{
    {
        using time_point = std::chrono::time_point<std::chrono::steady_clock>;

        status_mousebutton mbn;
        mbn = mcs::vulkan::event::mousebutton_event{}; // OK 语法支持
    }
    try
    {
        surface window{};
        window.setup({.width = 800, .height = 600}, "test"); // NOLINT

        glfw_input input;

        // 创建事件过滤器 - 修复返回类型问题
        auto ctrl_filter = [](const glfw_input &input) noexcept -> bool {
            // 检查左Ctrl或右Ctrl是否按下
            const auto &left_ctrl = input.get_keyboard_event(Key::LEFT_CONTROL);
            const auto &right_ctrl = input.get_keyboard_event(Key::RIGHT_CONTROL);
            return left_ctrl.press() || left_ctrl.repeat() || right_ctrl.press() ||
                   right_ctrl.repeat();
        };

        // NOTE: event_filter 是不需要的
        auto s_filter = event_filter([](const glfw_input &input) noexcept -> bool {
            const auto &s_key = input.get_keyboard_event(Key::S);
            return s_key.press();
        });

        auto mouse_filter = [](const glfw_input &input) noexcept -> bool {
            const auto &mouse_left =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);
            return mouse_left.press();
        };

        // 创建事件窗口：500毫秒内需要同时满足Ctrl+S+鼠标左键
        status_mousebutton my_btn{};
        auto my_mouse_filter = [&](const glfw_input &input) noexcept -> bool {
            const auto &mouse_left =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);
            my_btn = mouse_left;
            return my_btn.press() || my_btn.repeat();
        };

        using std::chrono::milliseconds;
        using std::chrono::steady_clock;
        constexpr int TARGET_FPS = 60;
        constexpr milliseconds FRAME_TIME(1000 / TARGET_FPS);

        auto lastFrameTime = steady_clock::now();
        auto lastFPSTime = steady_clock::now();
        int frameCount = 0;
        double fps = 0.0;

        while (window.shouldClose() == 0)
        {
            frameCount++;

            // input.reset();
            surface::pollEvents();

            // 原有的单键检测
            // NOTE:得更新
            // if (my_mouse_filter(input))
            // {
            //     std::cout << "my_btn.press() || my_btn.repeat()" << '\n';
            // }
            // const auto &s_key = input.get_keyboard_event(Key::S);
            // if (s_key.press() || s_key.repeat())
            // {
            //     std::cout << "s_key.press() || s_key.repeat()" << '\n';
            // }
            const auto &b_key = input.get_keyboard_event(Key::B);
            if ((b_key.press() || b_key.repeat()))
            {
                std::cout << " b" << '\n';
            }
            if (mouse_filter(input))
            {
                std::cout << "mouse_left" << '\n';
            }
            if (ctrl_filter(input))
            {
                std::cout << "ctrl" << '\n';
                if ((b_key.press() || b_key.repeat()))
                {
                    std::cout << "ctrl + b" << '\n';
                    if (mouse_filter(input))
                    {
                        std::cout << "ctrl + b + mouse_left" << '\n';
                    }
                }
            }
            // NOTE: 不去掉状态才是对的

            if (window.framebufferResized())
            {
                std::cout << ">>>>>>>>>>>>>>>>>>. Framebuffer resized!" << '\n';
                window.refFramebufferResized() = false;
            }

            // 帧率限制
            auto currentTime = steady_clock::now();
            auto elapsed = currentTime - lastFrameTime;

            if (elapsed < FRAME_TIME)
            {
                std::this_thread::sleep_for(FRAME_TIME - elapsed);
            }
            lastFrameTime = steady_clock::now();

            // 计算并打印FPS（每秒一次）
            auto elapsedSinceLastFPS =
                std::chrono::duration_cast<milliseconds>(currentTime - lastFPSTime);
            if (elapsedSinceLastFPS.count() >= 1000) // 1秒 = 1000毫秒
            {
                fps = frameCount * 1000.0 / elapsedSinceLastFPS.count();
                std::cout << "FPS: " << static_cast<int>(fps + 0.5)
                          << " (Target: " << TARGET_FPS << ")" << std::endl;

                frameCount = 0;
                lastFPSTime = currentTime;
            }
        }
        window.teardown();
    }
    catch (std::exception &e)
    {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    std::cout << "main done\n";
    return 0;
}
// NOLINTEND