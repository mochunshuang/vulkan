#pragma once

#include "input_interface.hpp"
#include "../event/keyboard_event_dispatcher.hpp"
#include "../event/mousebutton_event_dispatcher.hpp"
#include "../event/scroll_event_dispatcher.hpp"
#include "../event/cursor_pos_event_dispatcher.hpp"
#include "../event/cursor_enter_event_dispatcher.hpp"
#include <array>
#include <cstdint>
#include <print>
#include <utility>

namespace mcs::vulkan::input
{
    struct glfw_input : input_interface
    {
        constexpr glfw_input()
        {
            event::keyboard_event_dispatcher::instance().subscribe(
                this, &glfw_input::onKeyboardEvent);
            event::mousebutton_event_dispatcher::instance().subscribe(
                this, &glfw_input::onMouseButtonEvent);
            event::scroll_event_dispatcher::instance().subscribe(
                this, &glfw_input::onScrollEvent);
            event::cursor_pos_event_dispatcher::instance().subscribe(
                this, &glfw_input::onCursorPosEvent);
            event::cursor_enter_event_dispatcher::instance().subscribe(
                this, &glfw_input::onCursorEnter);
        }
        constexpr ~glfw_input() noexcept
        {
            event::keyboard_event_dispatcher::instance().unsubscribe(
                this, &glfw_input::onKeyboardEvent);
            event::mousebutton_event_dispatcher::instance().unsubscribe(
                this, &glfw_input::onMouseButtonEvent);
            event::scroll_event_dispatcher::instance().unsubscribe(
                this, &glfw_input::onScrollEvent);
            event::cursor_pos_event_dispatcher::instance().unsubscribe(
                this, &glfw_input::onCursorPosEvent);
            event::cursor_enter_event_dispatcher::instance().unsubscribe(
                this, &glfw_input::onCursorEnter);
        }
        glfw_input(const glfw_input &) = default;
        glfw_input(glfw_input &&) = default;
        glfw_input &operator=(const glfw_input &) = default;
        glfw_input &operator=(glfw_input &&) = default;

        static void onKeyboardEvent(void *self, keyboard_event key) noexcept
        {
            // std::println("key: {}", key);
            // NOLINTNEXTLINE
            static_cast<glfw_input *>(self)->keyboards_[static_cast<uint8_t>(key.key)] =
                key;
        }
        static void onMouseButtonEvent(void *self, mousebutton_event mouse) noexcept
        {
            std::println("mouse: {}", mouse);
            // NOLINTNEXTLINE
            static_cast<glfw_input *>(self)
                ->mousebuttons_[static_cast<uint8_t>(mouse.button)] = mouse;
        }
        static void onScrollEvent(void *self, scroll_event scroll) noexcept
        {
            // std::println("scroll: {}", scroll);
            static_cast<glfw_input *>(self)->scroll_ = std::move(scroll); // NOLINT
        }
        static void onCursorPosEvent(void *self, position2d_event pos) noexcept
        {
            // std::println("cursorPos: {}", pos);
            static_cast<glfw_input *>(self)->cursorPos_ = std::move(pos); // NOLINT
        }
        static void onCursorEnter(void *self, cursor_enter_event enter) noexcept
        {
            std::println("cursorEnter: {}", enter);
            static_cast<glfw_input *>(self)->cursorEnter_ = std::move(enter); // NOLINT
        }

        [[nodiscard]] const auto &keyboards() const noexcept
        {
            return keyboards_;
        }

        [[nodiscard]] const scroll_event &scroll() const noexcept
        {
            return scroll_;
        }

        [[nodiscard]] const position2d_event &cursorPos() const noexcept
        {
            return cursorPos_;
        }
        [[nodiscard]] const cursor_enter_event &cursorEnter() const noexcept
        {
            return cursorEnter_;
        }

        // NOLINTBEGIN
        constexpr void resetKeyboards() noexcept
        {
            keyboards_ = {};
        }
        constexpr void resetMousebuttons() noexcept
        {
            mousebuttons_ = {};
        }

        [[nodiscard]] const auto &get_keyboard_event(const event::Key &key) const noexcept
        {
            return keyboards_[static_cast<keyboard_event::key_store_type>(key)]; // NOLINT
        }
        [[nodiscard]] auto &get_keyboard_event(const event::Key &key) noexcept // NOLINT
        {
            return keyboards_[static_cast<keyboard_event::key_store_type>(key)]; // NOLINT
        }

        [[nodiscard]] const auto &get_mousebutton_event(
            const event::MouseButtons &btn) const noexcept
        {
            return mousebuttons_[static_cast<mousebutton_event::key_store_type>(btn)];
        }
        [[nodiscard]] auto &get_mousebutton_event(const event::MouseButtons &btn) noexcept
        {
            return mousebuttons_[static_cast<mousebutton_event::key_store_type>(btn)];
        }

        // NOLINTEND

      private:
        std::array<keyboard_event, static_cast<uint8_t>(event::Key::SIZE)> keyboards_;
        std::array<mousebutton_event, static_cast<uint8_t>(event::MouseButtons::SIZE)>
            mousebuttons_;
        scroll_event scroll_;
        position2d_event cursorPos_;
        cursor_enter_event cursorEnter_;
    };

}; // namespace mcs::vulkan::input
