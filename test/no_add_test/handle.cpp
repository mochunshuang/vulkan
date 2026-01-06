#include <cassert>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <utility>

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

struct register_fun
{
    using MouseButtonCallback = void (*)(void *, MouseButtons) noexcept;

    static auto &mouseButtonsCallbacks() noexcept
    {
        static std::unordered_map<void *, MouseButtonCallback> cllbacks;

        return cllbacks;
    }
    static void registerOnMouseButtonCallback(void *ptr, MouseButtonCallback fun_ptr)
    {
        mouseButtonsCallbacks().emplace(ptr, fun_ptr);
    }
    static void unregisterOnMouseButtonCallback(void *ptr)
    {
        mouseButtonsCallbacks().erase(ptr);
    }
};

static void notify_mouse(MouseButtons btn) noexcept // NOLINT
{
    for (auto &[ptr, callback] : register_fun::mouseButtonsCallbacks())
        callback(ptr, btn);
}

template <typename T> // NOLINTNEXTLINE
concept enable_onMouseButton = requires(T &t, MouseButtons mouseButton) {
    { t.onMouseButton(mouseButton) } noexcept -> std::same_as<void>;
};

template <typename T>
struct interface
{
  private:
    constexpr interface()
    {
        if constexpr (enable_onMouseButton<T>)
            register_fun::registerOnMouseButtonCallback(static_cast<void *>(this),
                                                        &onMouseButton<T>);
    }
    constexpr ~interface() noexcept
    {
        if constexpr (enable_onMouseButton<T>)
            register_fun::unregisterOnMouseButtonCallback(static_cast<void *>(this));
    }

  public:
    interface(const interface &) = delete;
    interface(interface &&) = delete;
    interface &operator=(const interface &) = delete;
    interface &operator=(interface &&) = delete;
    template <typename Impl>
    static void onMouseButton(void *ptr, MouseButtons mouseButton) noexcept
    {
        static_cast<Impl *>(ptr)->onMouseButton(std::move(mouseButton));
    }
    friend T;
};

struct myhanle : public interface<myhanle>
{
    myhanle() = default;
    void onMouseButton(MouseButtons mouseButton) noexcept
    {
        using enum MouseButtons;
        if (mouseButton == MOUSE_BUTTON_LAST)
        {
            std::cout << "MOUSE_BUTTON_LAST \n";
        }
        mouseButton_ = mouseButton;
    }

    MouseButtons mouseButton_{}; // NOLINT
};
static_assert(enable_onMouseButton<myhanle>);

struct myhanle1 : public interface<myhanle1>
{
    void onMouseButton(MouseButtons /*unused*/) {}
};
static_assert(not enable_onMouseButton<myhanle1>);

int main()
{
    // test 0 -----------------------------------------
    {
        myhanle m{};
        myhanle1 m1;
        assert(register_fun::mouseButtonsCallbacks().size() == 1);

        assert(m.mouseButton_ == MouseButtons{});
        notify_mouse(MouseButtons::MOUSE_BUTTON_LEFT);
        assert(m.mouseButton_ == MouseButtons::MOUSE_BUTTON_LEFT);
    }
    assert(register_fun::mouseButtonsCallbacks().size() == 0);
    // test 0 end-----------------------------------------

    std::cout << "main done\n";
    return 0;
}