#include "./head.hpp"
#include <cstddef>
#include <iostream>
#include <chrono>
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

template <typename F>
struct event_filter
{
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;

    bool pass{false};
    F fn;

    // 使用完美转发构造 - 添加推导指南
    template <typename Func>
        requires std::is_invocable_r_v<bool, Func, const glfw_input &>
    constexpr explicit event_filter(Func &&func) noexcept : fn(std::forward<Func>(func))
    {
    }

    constexpr void filter(const glfw_input &input) noexcept
    {
        static_assert(noexcept(fn(input)), "filter function must be noexcept");
        pass = fn(input);
    }

    // 调用运算符重载，方便使用
    constexpr void operator()(const glfw_input &input) noexcept
    {
        filter(input);
    }
};

// 添加推导指南
template <typename Func>
event_filter(Func) -> event_filter<Func>;

template <size_t availd_interval_ms, typename... Events>
struct event_window
{
  public:
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    using duration = std::chrono::milliseconds;

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

    // 默认构造函数
    constexpr event_window()
        : start_{std::chrono::steady_clock::now()}, status_{status::no_pass}
    {
    }

    void update(const glfw_input &input, time_point cur)
    {
        if (status_ == status::pass_but_no_consume)
            return;

        // 检查是否超时
        if (cur - start_ >= duration(availd_interval_ms))
        {
            start_ = cur;
            reset_by_timeout();
        }

        // 更新所有过滤器
        update_impl(input, std::index_sequence_for<Events...>{});

        // 检查是否所有过滤器都通过
        if (all_pass())
        {
            status_ = status::pass;
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
    time_point start_;
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
    try
    {
        surface window{};
        window.setup({.width = 800, .height = 600}, "test"); // NOLINT

        glfw_input input;

        // 创建事件过滤器 - 修复返回类型问题
        auto ctrl_filter = event_filter([](const glfw_input &input) noexcept -> bool {
            // 检查左Ctrl或右Ctrl是否按下
            const auto &left_ctrl = input.get_keyboard_event(Key::LEFT_CONTROL);
            const auto &right_ctrl = input.get_keyboard_event(Key::RIGHT_CONTROL);
            return left_ctrl.press() || right_ctrl.press();
        });

        auto s_filter = event_filter([](const glfw_input &input) noexcept -> bool {
            const auto &s_key = input.get_keyboard_event(Key::S);
            return s_key.press();
        });

        auto mouse_filter = event_filter([](const glfw_input &input) noexcept -> bool {
            const auto &mouse_left =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);
            return mouse_left.press();
        });

        // 创建事件窗口：500毫秒内需要同时满足Ctrl+S+鼠标左键
        auto combo_window =
            event_window<1000, decltype(ctrl_filter), decltype(s_filter),
                         decltype(mouse_filter)>(ctrl_filter, s_filter, mouse_filter);

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

            input.resetKeyboards();
            input.resetMousebuttons();
            surface::pollEvents();

            // 更新组合事件检测
            combo_window.update(input);

            if (combo_window.is_pass())
            {
                std::cout << ">>>>>>>>>>>>>>>>>>. Combo detected! Ctrl + S + Mouse Left"
                          << '\n';
                combo_window.consume(); // 消费事件
            }

            // 原有的单键检测
            const auto &key_b_event = input.get_keyboard_event(Key::B);
            if (key_b_event.press() && key_b_event.isModifier(ModifierKey::CtrlAlt()))
            {
                std::cout << ">>>>>>>>>>>>>>>>>>. ctrl + alt + b\n";
            }
            if (key_b_event.release())
            {
                std::cout << ">>>>>>>>>>>>>>>>>>. release  b\n";
            }

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