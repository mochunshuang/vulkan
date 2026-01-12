#pragma once

#include "../event/event_type.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace mcs::vulkan::camera
{

    // NOLINTBEGIN
    struct camera_controller
    {
      public:
        enum class Mode
        {
            FirstPerson, // 第一人称
            Orbit,       // 轨道相机（观察模式）
            FreeLook     // 自由视角
        };

        struct CameraState
        {
            glm::vec3 position{0.0f, 0.0f, 5.0f};
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 target{0.0f, 0.0f, 0.0f};

            float yaw{-90.0f}; // 偏航角
            float pitch{0.0f}; // 俯仰角
            float roll{0.0f};  // 滚动角

            glm::vec3 front{0.0f, 0.0f, -1.0f};
            glm::vec3 right{1.0f, 0.0f, 0.0f};
            glm::vec3 up{0.0f, 1.0f, 0.0f};

            float fov{45.0f};           // 视野角度
            float aspect{16.0f / 9.0f}; // 宽高比
            float nearPlane{0.1f};      // 近平面
            float farPlane{1000.0f};    // 远平面

            bool updated{true};
        };

        camera_controller() = default;

        explicit camera_controller(Mode mode) : mode_(mode) {}

        void set_mode(Mode mode) noexcept
        {
            mode_ = mode;
        }

        void set_position(const glm::vec3 &position) noexcept
        {
            state_.position = position;
            state_.updated = true;
        }

        void set_target(const glm::vec3 &target) noexcept
        {
            state_.target = target;
            state_.updated = true;
        }

        void set_fov(float fov) noexcept
        {
            state_.fov = glm::clamp(fov, 1.0f, 120.0f);
            state_.updated = true;
        }

        void set_aspect_ratio(float aspect) noexcept
        {
            state_.aspect = aspect;
            state_.updated = true;
        }

        void set_clip_planes(float nearPlane, float farPlane) noexcept
        {
            state_.nearPlane = nearPlane;
            state_.farPlane = farPlane;
            state_.updated = true;
        }

        void set_movement_speed(float speed) noexcept
        {
            movement_speed_ = speed;
        }

        void set_rotation_speed(float speed) noexcept
        {
            rotation_speed_ = speed;
        }

        void update(const auto &input, float delta_time)
        {
            switch (mode_)
            {
            case Mode::FirstPerson:
                update_first_person(input, delta_time);
                break;
            case Mode::Orbit:
                update_orbit(input, delta_time);
                break;
            case Mode::FreeLook:
                update_free_look(input, delta_time);
                break;
            }

            update_camera_vectors();
        }

        glm::mat4 get_view_matrix() const noexcept
        {
            return glm::lookAt(state_.position, state_.position + state_.front,
                               state_.up);
        }

        glm::mat4 get_projection_matrix() const noexcept
        {
            return glm::perspective(glm::radians(state_.fov), state_.aspect,
                                    state_.nearPlane, state_.farPlane);
        }

        glm::mat4 get_view_projection_matrix() const noexcept
        {
            return get_projection_matrix() * get_view_matrix();
        }

        const CameraState &get_state() const noexcept
        {
            return state_;
        }

        bool has_updated() const noexcept
        {
            return state_.updated;
        }

        void reset_updated() noexcept
        {
            state_.updated = false;
        }

      private:
        void update_first_person(const auto &input, float delta_time)
        {
            handle_keyboard_movement(input, delta_time);
            handle_mouse_look(input, delta_time);
            handle_mouse_scroll(input);
        }

        void update_orbit(const auto &input, float delta_time)
        {
            handle_orbit_rotation(input, delta_time);
            handle_orbit_zoom(input, delta_time);
            handle_orbit_pan(input, delta_time);
        }

        void update_free_look(const auto &input, float delta_time)
        {
            handle_keyboard_movement(input, delta_time);
            handle_mouse_look(input, delta_time);
            handle_mouse_scroll(input);
        }

        void handle_keyboard_movement(const auto &input, float delta_time)
        {
            float velocity = movement_speed_ * delta_time;

            // 检查WASD键
            auto is_key_pressed = [&](event::Key key) {
                const auto &key_event = input.get_keyboard_event(key);
                return key_event.press() || key_event.repeat();
            };

            if (is_key_pressed(event::Key::W))
            {
                state_.position += state_.front * velocity;
                state_.updated = true;
            }
            if (is_key_pressed(event::Key::S))
            {
                state_.position -= state_.front * velocity;
                state_.updated = true;
            }
            if (is_key_pressed(event::Key::A))
            {
                state_.position -= state_.right * velocity;
                state_.updated = true;
            }
            if (is_key_pressed(event::Key::D))
            {
                state_.position += state_.right * velocity;
                state_.updated = true;
            }
            if (is_key_pressed(event::Key::Q))
            {
                state_.position -= state_.up * velocity;
                state_.updated = true;
            }
            if (is_key_pressed(event::Key::E))
            {
                state_.position += state_.up * velocity;
                state_.updated = true;
            }
        }

        void handle_mouse_look(const auto &input, float delta_time)
        {
            const auto &mouse_pos = input.cursorPos();

            if (first_mouse_)
            {
                last_mouse_x_ = mouse_pos.xpos;
                last_mouse_y_ = mouse_pos.ypos;
                first_mouse_ = false;
            }

            float xoffset = mouse_pos.xpos - last_mouse_x_;
            float yoffset = last_mouse_y_ - mouse_pos.ypos; // 反转Y轴

            last_mouse_x_ = mouse_pos.xpos;
            last_mouse_y_ = mouse_pos.ypos;

            // 检查鼠标右键是否按下
            const auto &mouse_right =
                input.get_mousebutton_event(event::MouseButtons::MOUSE_BUTTON_RIGHT);

            if (mouse_right.press() || mouse_right.repeat())
            {
                xoffset *= rotation_speed_ * delta_time;
                yoffset *= rotation_speed_ * delta_time;

                state_.yaw += xoffset;
                state_.pitch += yoffset;

                // 限制俯仰角
                state_.pitch = glm::clamp(state_.pitch, -89.0f, 89.0f);

                state_.updated = true;
            }
        }

        void handle_mouse_scroll(const auto &input)
        {
            const auto &scroll = input.scroll();

            state_.fov -= static_cast<float>(scroll.yoffset);
            state_.fov = glm::clamp(state_.fov, 1.0f, 120.0f);
            state_.updated = true;
        }

        void handle_orbit_rotation(const auto &input, float delta_time)
        {
            const auto &mouse_pos = input.cursorPos();
            const auto &mouse_left =
                input.get_mousebutton_event(event::MouseButtons::MOUSE_BUTTON_LEFT);

            if (mouse_left.press() || mouse_left.repeat())
            {
                float xoffset = mouse_pos.xpos - last_mouse_x_;
                float yoffset = mouse_pos.ypos - last_mouse_y_;

                xoffset *= orbit_rotation_speed_ * delta_time;
                yoffset *= orbit_rotation_speed_ * delta_time;

                // 计算轨道角度
                orbit_yaw_ += xoffset;
                orbit_pitch_ += yoffset;
                orbit_pitch_ = glm::clamp(orbit_pitch_, -89.0f, 89.0f);

                // 更新相机位置
                float radius = glm::length(state_.position - state_.target);
                state_.position.x =
                    state_.target.x + radius * cos(glm::radians(orbit_pitch_)) *
                                          sin(glm::radians(orbit_yaw_));
                state_.position.y =
                    state_.target.y + radius * sin(glm::radians(orbit_pitch_));
                state_.position.z =
                    state_.target.z + radius * cos(glm::radians(orbit_pitch_)) *
                                          cos(glm::radians(orbit_yaw_));

                state_.updated = true;
            }

            last_mouse_x_ = mouse_pos.xpos;
            last_mouse_y_ = mouse_pos.ypos;
        }

        void handle_orbit_zoom(const auto &input, float delta_time)
        {
            const auto &scroll = input.scroll();

            if (scroll.yoffset != 0.0)
            {
                float zoom_amount =
                    static_cast<float>(scroll.yoffset) * orbit_zoom_speed_ * delta_time;
                glm::vec3 dir = glm::normalize(state_.position - state_.target);
                state_.position += dir * zoom_amount;

                // 限制最小距离
                float distance = glm::length(state_.position - state_.target);
                if (distance < orbit_min_distance_)
                {
                    state_.position = state_.target + dir * orbit_min_distance_;
                }

                state_.updated = true;
            }
        }

        void handle_orbit_pan(const auto &input, float delta_time)
        {
            const auto &mouse_middle =
                input.get_mousebutton_event(event::MouseButtons::MOUSE_BUTTON_MIDDLE);
            const auto &mouse_pos = input.cursorPos();

            if (mouse_middle.press() || mouse_middle.repeat())
            {
                float xoffset = mouse_pos.xpos - last_mouse_x_;
                float yoffset = mouse_pos.ypos - last_mouse_y_;

                glm::vec3 pan_right = glm::normalize(glm::cross(state_.front, state_.up));
                glm::vec3 pan_up = state_.up;

                state_.position -= pan_right * xoffset * orbit_pan_speed_ * delta_time;
                state_.position += pan_up * yoffset * orbit_pan_speed_ * delta_time;
                state_.target -= pan_right * xoffset * orbit_pan_speed_ * delta_time;
                state_.target += pan_up * yoffset * orbit_pan_speed_ * delta_time;

                state_.updated = true;
            }
        }

        void update_camera_vectors()
        {
            // 根据欧拉角计算前向量
            glm::vec3 front;
            front.x = cos(glm::radians(state_.yaw)) * cos(glm::radians(state_.pitch));
            front.y = sin(glm::radians(state_.pitch));
            front.z = sin(glm::radians(state_.yaw)) * cos(glm::radians(state_.pitch));
            state_.front = glm::normalize(front);

            // 计算右向量和上向量
            state_.right = glm::normalize(glm::cross(state_.front, world_up_));
            state_.up = glm::normalize(glm::cross(state_.right, state_.front));

            // 更新旋转四元数
            glm::quat pitch =
                glm::angleAxis(glm::radians(state_.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat yaw =
                glm::angleAxis(glm::radians(state_.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            state_.rotation = yaw * pitch;
        }

      private:
        CameraState state_;
        Mode mode_{Mode::FirstPerson};

        // 控制参数
        float movement_speed_{2.5f};
        float rotation_speed_{0.1f};

        // 轨道相机参数
        float orbit_yaw_{0.0f};
        float orbit_pitch_{0.0f};
        float orbit_rotation_speed_{50.0f};
        float orbit_zoom_speed_{5.0f};
        float orbit_pan_speed_{0.5f};
        float orbit_min_distance_{0.5f};

        // 鼠标状态
        bool first_mouse_{true};
        double last_mouse_x_{0.0};
        double last_mouse_y_{0.0};

        const glm::vec3 world_up_{0.0f, 1.0f, 0.0f};
    };

    // NOLINTEND

} // namespace mcs::vulkan::camera