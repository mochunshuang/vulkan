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

struct camera_base
{
    void update(glfw_input &input)
    {
        namespace event = mcs::vulkan::event;
        if (input.acceptNewEvent())
        {
            // NOTE: 更新
            if (input.scroll() != event::scroll_event{})
            {
                event::scroll_event scroll = input.scroll();
                std::println("scroll: {}", scroll);
            }

            if (input.cursorPos() != event::position2d_event{})
            {
                event::position2d_event cursorPos = input.cursorPos();
                std::println("cursorPos: {}", cursorPos);
            }

            const auto is_key_pressed = [&](event::Key key) constexpr noexcept {
                const auto &key_event = input.get_keyboard_event(key);
                return key_event.press() || key_event.repeat();
            };
            const auto is_mouse_pressed =
                [&](event::MouseButtons key) constexpr noexcept {
                    const auto &key_event = input.get_mousebutton_event(key);
                    return key_event.press() || key_event.repeat();
                };
            if (is_key_pressed(event::Key::W))
            {
                std::println("key_pressed: W ");
            }
            if (is_key_pressed(event::Key::S))
            {
                std::println("key_pressed: S ");
            }
            if (is_key_pressed(event::Key::A))
            {
                std::println("key_pressed: A ");
            }
            if (is_key_pressed(event::Key::D))
            {
                std::println("key_pressed: D ");
            }
            if (is_key_pressed(event::Key::Q))
            {
                std::println("key_pressed: Q ");
            }
            if (is_key_pressed(event::Key::E))
            {
                std::println("key_pressed: E ");
            }

            const auto &mouse_btn_left =
                input.get_mousebutton_event(event::MouseButtons::MOUSE_BUTTON_LEFT);
            if (mouse_btn_left != event::mousebutton_event{})
            {
                // 新的事件是 mouse_btn_left 的
                if (mouse_btn_left.press())
                {
                    std::println("mouse_btn_left: pressed");
                }
                if (mouse_btn_left.release())
                {
                    std::println("mouse_btn_left: released");
                }

                /*
                // NOTE: 下面的代码不会出现。因此。按住鼠标左键移动的时候。
                mouse_btn_left: pressed
                cursorPos: position2d_event{xpos=548.00, ypos=173.00}
                cursorPos: position2d_event{xpos=547.00, ypos=173.00}
                cursorPos: position2d_event{xpos=546.00, ypos=173.00}
                cursorPos: position2d_event{xpos=545.00, ypos=173.00}
                mouse: mousebutton_event{button=MOUSE_BUTTON_LEFT, action=RELEASE,
                       modifier=NONE}
                mouse_btn_left: released
                */
                if (mouse_btn_left.repeat())
                {
                    std::println("mouse_btn_left: repeated"); // NOTE: 永远不会打印
                }
            }

            const auto &mouse_btn_right =
                input.get_mousebutton_event(event::MouseButtons::MOUSE_BUTTON_RIGHT);
            if (mouse_btn_right != event::mousebutton_event{})
            {
                if (mouse_btn_right.press())
                {
                    std::println("mouse_btn_right: pressed");
                }
                if (mouse_btn_right.release())
                {
                    std::println("mouse_btn_right: released");
                }

                /*
                // NOTE: 下面的代码不会出现。因此。按住鼠标左键移动的时候。
    mouse_btn_right: pressed
    cursorPos: position2d_event{xpos=430.00, ypos=102.00}
    cursorPos: position2d_event{xpos=428.00, ypos=102.00}
    cursorPos: position2d_event{xpos=428.00, ypos=103.00}
    cursorPos: position2d_event{xpos=427.00, ypos=103.00}
    cursorPos: position2d_event{xpos=426.00, ypos=103.00}
    cursorPos: position2d_event{xpos=425.00, ypos=103.00}
    cursorPos: position2d_event{xpos=424.00, ypos=103.00}
    mouse: mousebutton_event{button=MOUSE_BUTTON_RIGHT, action=RELEASE, modifier=NONE}
    mouse_btn_right: released
                */
                if (mouse_btn_left.repeat())
                {
                    std::println("mouse_btn_right: repeated"); // NOTE: 永远不会打印
                }
            }

            const auto &mouse_btn_middle =
                input.get_mousebutton_event(event::MouseButtons::MOUSE_BUTTON_MIDDLE);
            if (mouse_btn_middle != event::mousebutton_event{})
            {
                if (mouse_btn_middle.press())
                {
                    std::println("mouse_btn_middle: pressed");
                }
                if (mouse_btn_middle.release())
                {
                    std::println("mouse_btn_middle: released");
                }
                if (mouse_btn_middle.repeat())
                {
                    std::println("mouse_btn_right: repeated"); // NOTE: 永远不会打印
                }
            }
        }
    }
};

int main()
{
    try
    {
        surface window{};
        window.setup({.width = 800, .height = 600}, "test"); // NOLINT

        glfw_input input;
        camera_base camera_;

        while (window.shouldClose() == 0)
        {
            input.reset();
            surface::pollEvents();
            camera_.update(input);

            if (window.framebufferResized())
            {
                std::cout << ">>>>>>>>>>>>>>>>>>. Framebuffer resized!" << '\n';
                window.refFramebufferResized() = false;
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
/*
PS E:\0_github_project\vulkan\output\test_program\framework>
."E:/0_github_project/vulkan/output/test_program/framework/framework-test_input_camera.exe"
cursorEnter: cursor_enter_event{entered=true}
cursorPos: position2d_event{xpos=481.00, ypos=137.00}
key_pressed: A
key_pressed: S
key_pressed: D
key_pressed: D
key_pressed: D
key_pressed: D
key_pressed: D
scroll: scroll_event{xoffset=0.00, yoffset=-1.00}
scroll: scroll_event{xoffset=0.00, yoffset=-1.00}
scroll: scroll_event{xoffset=0.00, yoffset=-1.00}
mouse: mousebutton_event{button=MOUSE_BUTTON_RIGHT, action=PRESS, modifier=NONE}
mouse_btn_right: pressed
mouse: mousebutton_event{button=MOUSE_BUTTON_RIGHT, action=RELEASE, modifier=NONE}
mouse_btn_right: released
mouse: mousebutton_event{button=MOUSE_BUTTON_RIGHT, action=PRESS, modifier=NONE}
mouse_btn_right: pressed
cursorPos: position2d_event{xpos=480.00, ypos=137.00}
cursorPos: position2d_event{xpos=479.00, ypos=137.00}
cursorPos: position2d_event{xpos=478.00, ypos=137.00}
cursorPos: position2d_event{xpos=477.00, ypos=137.00}
cursorPos: position2d_event{xpos=476.00, ypos=137.00}
cursorPos: position2d_event{xpos=476.00, ypos=138.00}
cursorPos: position2d_event{xpos=475.00, ypos=138.00}
cursorPos: position2d_event{xpos=474.00, ypos=139.00}
cursorPos: position2d_event{xpos=473.00, ypos=139.00}
mouse: mousebutton_event{button=MOUSE_BUTTON_RIGHT, action=RELEASE, modifier=NONE}
mouse_btn_right: released
cursorPos: position2d_event{xpos=472.00, ypos=139.00}
cursorPos: position2d_event{xpos=462.00, ypos=140.00}
cursorPos: position2d_event{xpos=461.00, ypos=140.00}
cursorPos: position2d_event{xpos=460.00, ypos=140.00}
cursorPos: position2d_event{xpos=459.00, ypos=140.00}
cursorPos: position2d_event{xpos=458.00, ypos=140.00}
cursorPos: position2d_event{xpos=457.00, ypos=140.00}
cursorPos: position2d_event{xpos=456.00, ypos=140.00}
cursorPos: position2d_event{xpos=455.00, ypos=140.00}
mouse: mousebutton_event{button=MOUSE_BUTTON_LEFT, action=PRESS, modifier=NONE}
mouse_btn_left: pressed
cursorPos: position2d_event{xpos=454.00, ypos=140.00}
cursorPos: position2d_event{xpos=453.00, ypos=140.00}
cursorPos: position2d_event{xpos=452.00, ypos=140.00}
cursorPos: position2d_event{xpos=451.00, ypos=140.00}
cursorPos: position2d_event{xpos=450.00, ypos=140.00}
cursorPos: position2d_event{xpos=449.00, ypos=140.00}
cursorPos: position2d_event{xpos=448.00, ypos=140.00}
cursorPos: position2d_event{xpos=447.00, ypos=140.00}
mouse: mousebutton_event{button=MOUSE_BUTTON_LEFT, action=RELEASE, modifier=NONE}
mouse_btn_left: released
[ DEBUG ] [glfw.hpp:237:static void __cdecl
mcs::vulkan::wsi::glfw::Window::keyCallback(GLFWwindow *, int, int, int, int)]: 按下了 ESC
键，退出程序 main done
*/
// NOLINTEND