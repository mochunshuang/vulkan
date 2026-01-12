#include "./head.hpp"
#include <cstddef>
#include <iostream>
#include <chrono>
#include <print>
#include <thread>
#include <tuple>
#include <utility>

using surface = mcs::vulkan::wsi::glfw::Window;
using glfw_input = mcs::vulkan::input::glfw_input;
using Key = mcs::vulkan::event::Key;
using ModifierKey = mcs::vulkan::event::ModifierKey;
using MouseButtons = mcs::vulkan::event::MouseButtons;
using Action = mcs::vulkan::event::Action;

// NOLINTBEGIN

// camera.hpp
// camera.hpp
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <print>
#include <cassert>
#include <algorithm>
#include <optional>
#include <numbers>
#include "./head.hpp" // 包含输入系统头文件

namespace mcs::vulkan::camera
{

    // 简单的窗口尺寸结构体
    struct WindowSize
    {
        uint32_t width;
        uint32_t height;

        [[nodiscard]] float aspect_ratio() const noexcept
        {
            return static_cast<float>(width) / static_cast<float>(height);
        }

        [[nodiscard]] bool is_valid() const noexcept
        {
            return width > 0 && height > 0;
        }
    };

    enum class ProjectionType
    {
        Perspective,
        Orthographic
    };

    enum class CameraMode
    {
        FirstPerson,
        LookAt,
        Orbit,
        Free
    };

    enum class MovementAxis
    {
        Local, // 相对于相机本地坐标系移动
        World, // 相对于世界坐标系移动
        Screen // 相对于屏幕坐标系移动
    };

    struct CameraSettings
    {
        float field_of_view = 45.0f; // 透视投影的视野角度（度）
        float ortho_size = 5.0f;     // 正交投影的尺寸
        float near_plane = 0.1f;     // 近平面
        float far_plane = 1000.0f;   // 远平面

        float movement_speed = 5.0f;  // 移动速度
        float rotation_speed = 0.25f; // 旋转速度（度/像素）
        float zoom_speed = 1.0f;      // 缩放速度
        float pan_speed = 0.005f;     // 平移速度

        bool constrain_pitch = true;  // 限制俯仰角（防止万向锁）
        bool invert_y = false;        // 反转Y轴
        bool smooth_movement = false; // 平滑移动（默认关闭）
        float smooth_factor = 0.1f;   // 平滑系数

        float min_pitch = -89.0f;    // 最小俯仰角（度）
        float max_pitch = 89.0f;     // 最大俯仰角（度）
        float min_distance = 0.1f;   // 最小距离（轨道相机）
        float max_distance = 100.0f; // 最大距离（轨道相机）
    };

    // 基础相机类
    class Camera
    {
      public:
        Camera()
        {
            // 设置默认投影
            set_perspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        }

        virtual ~Camera() = default;

        // 基础变换
        virtual void set_position(const glm::vec3 &position)
        {
            position_ = position;
            view_dirty_ = true;
            print_transform("set_position");
        }

        virtual void move(const glm::vec3 &offset,
                          MovementAxis axis = MovementAxis::Local)
        {
            glm::vec3 actual_offset = offset;

            switch (axis)
            {
            case MovementAxis::Local:
                actual_offset = get_orientation() * offset;
                break;
            case MovementAxis::Screen:
                // 屏幕空间转换到世界空间
                actual_offset = get_right_vector() * offset.x +
                                get_up_vector() * offset.y +
                                get_forward_vector() * offset.z;
                break;
            case MovementAxis::World:
            default:
                break;
            }

            // 注意：这里不乘以速度，速度在控制器中处理
            position_ += actual_offset;
            view_dirty_ = true;
        }

        virtual void rotate(float yaw, float pitch, float roll = 0.0f)
        {
            // 更新欧拉角
            yaw_ += yaw;
            pitch_ += pitch * (settings_.invert_y ? -1.0f : 1.0f);
            roll_ += roll;

            // 限制俯仰角
            if (settings_.constrain_pitch)
            {
                pitch_ = std::clamp(pitch_, settings_.min_pitch, settings_.max_pitch);
            }

            // 更新四元数朝向
            update_orientation_from_euler();

            view_dirty_ = true;
        }

        virtual void look_at(const glm::vec3 &target)
        {
            target_ = target;

            // 计算朝向目标的方向
            glm::vec3 direction = glm::normalize(target - position_);

            // 从方向向量计算欧拉角
            yaw_ = glm::degrees(atan2(direction.x, direction.z));
            pitch_ = glm::degrees(asin(direction.y));

            // 限制俯仰角
            if (settings_.constrain_pitch)
            {
                pitch_ = std::clamp(pitch_, settings_.min_pitch, settings_.max_pitch);
            }

            // 更新四元数
            update_orientation_from_euler();

            view_dirty_ = true;
        }

        // 投影设置 - 保持4个参数的兼容性
        void set_perspective(float fov, float aspect, float near, float far)
        {
            projection_type_ = ProjectionType::Perspective;
            settings_.field_of_view = fov;
            aspect_ratio_ = aspect;
            settings_.near_plane = near;
            settings_.far_plane = far;
            projection_dirty_ = true;
            assert(fov > 0.0f && fov < 180.0f &&
                   "Field of view must be between 0 and 180 degrees");
            print_projection("set_perspective");
        }

        void set_orthographic(float size, float aspect, float near, float far)
        {
            projection_type_ = ProjectionType::Orthographic;
            settings_.ortho_size = size;
            aspect_ratio_ = aspect;
            settings_.near_plane = near;
            settings_.far_plane = far;
            projection_dirty_ = true;
            assert(size > 0.0f && "Orthographic size must be positive");
            print_projection("set_orthographic");
        }

        // 窗口和宽高比管理
        void set_window_size(const WindowSize &size)
        {
            if (!size.is_valid())
            {
                std::println("[Camera] Warning: Invalid window size ({}, {})", size.width,
                             size.height);
                return;
            }

            window_size_ = size;
            set_aspect_ratio(size.aspect_ratio());
        }

        void set_aspect_ratio(float aspect)
        {
            assert(aspect > 0.0f && "Aspect ratio must be positive");
            if (std::abs(aspect_ratio_ - aspect) > 0.001f)
            {
                aspect_ratio_ = aspect;
                projection_dirty_ = true;
            }
        }

        [[nodiscard]] float get_aspect_ratio() const noexcept
        {
            return aspect_ratio_;
        }

        [[nodiscard]] std::optional<WindowSize> get_window_size() const noexcept
        {
            return window_size_;
        }

        // 获取变换矩阵
        const glm::mat4 &get_view_matrix()
        {
            if (view_dirty_)
            {
                update_view_matrix();
            }
            return view_matrix_;
        }

        const glm::mat4 &get_projection_matrix()
        {
            if (projection_dirty_)
            {
                update_projection_matrix();
            }
            return projection_matrix_;
        }

        glm::mat4 get_view_projection_matrix()
        {
            return get_projection_matrix() * get_view_matrix();
        }

        // 获取相机属性
        [[nodiscard]] glm::vec3 get_position() const noexcept
        {
            return position_;
        }
        [[nodiscard]] glm::quat get_orientation() const noexcept
        {
            return orientation_;
        }
        [[nodiscard]] glm::vec3 get_forward_vector() const noexcept
        {
            return orientation_ * glm::vec3(0, 0, -1);
        }
        [[nodiscard]] glm::vec3 get_right_vector() const noexcept
        {
            return orientation_ * glm::vec3(1, 0, 0);
        }
        [[nodiscard]] glm::vec3 get_up_vector() const noexcept
        {
            return orientation_ * glm::vec3(0, 1, 0);
        }
        [[nodiscard]] glm::vec3 get_target() const noexcept
        {
            return target_;
        }

        [[nodiscard]] float get_yaw() const noexcept
        {
            return yaw_;
        }
        [[nodiscard]] float get_pitch() const noexcept
        {
            return pitch_;
        }
        [[nodiscard]] float get_roll() const noexcept
        {
            return roll_;
        }

        void set_yaw(float yaw)
        {
            yaw_ = yaw;
            update_orientation_from_euler();
            view_dirty_ = true;
        }

        void set_pitch(float pitch)
        {
            pitch_ = std::clamp(pitch, settings_.min_pitch, settings_.max_pitch);
            update_orientation_from_euler();
            view_dirty_ = true;
        }

        void set_roll(float roll)
        {
            roll_ = roll;
            update_orientation_from_euler();
            view_dirty_ = true;
        }

        [[nodiscard]] CameraMode get_mode() const noexcept
        {
            return mode_;
        }
        void set_mode(CameraMode mode)
        {
            mode_ = mode;
            if (mode == CameraMode::Orbit)
            {
                // 切换到轨道模式时，计算初始距离
                orbit_distance_ = glm::distance(position_, target_);
            }
        }

        [[nodiscard]] CameraSettings &get_settings() noexcept
        {
            return settings_;
        }
        [[nodiscard]] const CameraSettings &get_settings() const noexcept
        {
            return settings_;
        }

        [[nodiscard]] ProjectionType get_projection_type() const noexcept
        {
            return projection_type_;
        }

        // 射线投射（用于鼠标拾取）
        [[nodiscard]] glm::vec3 screen_to_world_ray(const glm::vec2 &screen_pos)
        {
            if (!window_size_)
            {
                return glm::vec3(0.0f, 0.0f, -1.0f);
            }

            // 归一化设备坐标 [-1, 1]
            float x = (2.0f * screen_pos.x) / window_size_->width - 1.0f;
            float y = 1.0f - (2.0f * screen_pos.y) / window_size_->height;

            glm::vec4 ray_clip(x, y, -1.0f, 1.0f);
            glm::vec4 ray_eye = glm::inverse(get_projection_matrix()) * ray_clip;
            ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

            glm::vec3 ray_world = glm::vec3(glm::inverse(get_view_matrix()) * ray_eye);
            return glm::normalize(ray_world);
        }

        // 重置相机
        void reset()
        {
            position_ = glm::vec3(0.0f, 0.0f, 5.0f);
            target_ = glm::vec3(0.0f, 0.0f, 0.0f);
            yaw_ = 0.0f;
            pitch_ = 0.0f;
            roll_ = 0.0f;
            orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            orbit_distance_ = 5.0f;
            view_dirty_ = true;
            std::println("[Camera] Camera reset");
        }

      protected:
        virtual void update_view_matrix()
        {
            switch (mode_)
            {
            case CameraMode::FirstPerson:
            case CameraMode::Free:
                view_matrix_ = glm::lookAt(position_, position_ + get_forward_vector(),
                                           get_up_vector());
                break;

            case CameraMode::LookAt:
                view_matrix_ = glm::lookAt(position_, target_, glm::vec3(0, 1, 0));
                break;

            case CameraMode::Orbit:
                update_orbit_position();
                view_matrix_ = glm::lookAt(position_, target_, glm::vec3(0, 1, 0));
                break;
            }
            view_dirty_ = false;
        }

        virtual void update_projection_matrix()
        {
            if (projection_type_ == ProjectionType::Perspective)
            {
                projection_matrix_ =
                    glm::perspective(glm::radians(settings_.field_of_view), aspect_ratio_,
                                     settings_.near_plane, settings_.far_plane);
            }
            else
            {
                float half_width = settings_.ortho_size * aspect_ratio_ * 0.5f;
                float half_height = settings_.ortho_size * 0.5f;
                projection_matrix_ =
                    glm::ortho(-half_width, half_width, -half_height, half_height,
                               settings_.near_plane, settings_.far_plane);
            }
            projection_dirty_ = false;
        }

        void update_orientation_from_euler()
        {
            // 转换为弧度
            float yaw_rad = glm::radians(yaw_);
            float pitch_rad = glm::radians(pitch_);
            float roll_rad = glm::radians(roll_);

            // 创建四元数（顺序：Yaw -> Pitch -> Roll）
            glm::quat q_yaw = glm::angleAxis(yaw_rad, glm::vec3(0, 1, 0));
            glm::quat q_pitch = glm::angleAxis(pitch_rad, glm::vec3(1, 0, 0));
            glm::quat q_roll = glm::angleAxis(roll_rad, glm::vec3(0, 0, 1));

            orientation_ = q_yaw * q_pitch * q_roll;
            orientation_ = glm::normalize(orientation_);
        }

        void update_orbit_position()
        {
            // 从球坐标计算位置
            float yaw_rad = glm::radians(yaw_);
            float pitch_rad = glm::radians(pitch_);

            // 限制距离
            orbit_distance_ = std::clamp(orbit_distance_, settings_.min_distance,
                                         settings_.max_distance);

            position_.x = target_.x + orbit_distance_ * cos(pitch_rad) * sin(yaw_rad);
            position_.y = target_.y + orbit_distance_ * sin(pitch_rad);
            position_.z = target_.z + orbit_distance_ * cos(pitch_rad) * cos(yaw_rad);
        }

        void print_transform(const char *operation)
        {
            std::println("[Camera] {}: Position({:.2f}, {:.2f}, {:.2f}), Forward({:.2f}, "
                         "{:.2f}, {:.2f})",
                         operation, position_.x, position_.y, position_.z,
                         get_forward_vector().x, get_forward_vector().y,
                         get_forward_vector().z);
        }

        void print_projection(const char *operation)
        {
            std::println("[Camera] {}: {} projection, FOV={:.1f}, Aspect={:.2f}, "
                         "Near={:.2f}, Far={:.2f}",
                         operation,
                         projection_type_ == ProjectionType::Perspective ? "Perspective"
                                                                         : "Orthographic",
                         settings_.field_of_view, aspect_ratio_, settings_.near_plane,
                         settings_.far_plane);
        }

      protected:
        // 位置和朝向
        glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 5.0f);
        glm::vec3 target_ = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::quat orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        // 欧拉角（度）
        float yaw_ = 0.0f;   // 偏航角
        float pitch_ = 0.0f; // 俯仰角
        float roll_ = 0.0f;  // 翻滚角

        // 轨道相机参数
        float orbit_distance_ = 5.0f;

        // 变换矩阵
        glm::mat4 view_matrix_;
        glm::mat4 projection_matrix_;

        // 状态
        CameraMode mode_ = CameraMode::FirstPerson;
        ProjectionType projection_type_ = ProjectionType::Perspective;
        CameraSettings settings_;

        // 宽高比和窗口大小
        float aspect_ratio_ = 16.0f / 9.0f;
        std::optional<WindowSize> window_size_;

        // 脏标记
        bool view_dirty_ = true;
        bool projection_dirty_ = true;
    };

    // 输入控制器 - 处理输入事件并控制相机
    class CameraController
    {
      public:
        explicit CameraController(std::shared_ptr<Camera> camera)
            : camera_(std::move(camera))
        {
            assert(camera_ != nullptr);
            std::println("[CameraController] Created with camera mode: {}",
                         camera_mode_to_string(camera_->get_mode()));
        }

        void update(glfw_input &input, float delta_time)
        {
            if (!camera_ || !enabled_)
                return;

            // 调试信息：打印输入事件
            debug_print_input_events(input);

            // 处理所有输入
            handle_keyboard(input, delta_time);
            handle_mouse(input, delta_time);
            handle_scroll(input);

            // 打印控制器状态（每秒一次）
            static float status_timer = 0.0f;
            status_timer += delta_time;
            if (status_timer > 1.0f)
            {
                std::println("[CameraController] Status: rotating={}, panning={}, "
                             "last_mouse_pos=({:.1f}, {:.1f})",
                             mouse_rotating_, mouse_panning_, last_mouse_pos_.x,
                             last_mouse_pos_.y);
                status_timer = 0.0f;
            }
        }

        void set_enabled(bool enabled)
        {
            enabled_ = enabled;
            std::println("[CameraController] {}", enabled ? "Enabled" : "Disabled");
        }

        bool is_enabled() const
        {
            return enabled_;
        }

        std::shared_ptr<Camera> get_camera()
        {
            return camera_;
        }

        void set_mouse_sensitivity(float sensitivity)
        {
            mouse_sensitivity_ = sensitivity;
            std::println("[CameraController] Mouse sensitivity: {:.2f}", sensitivity);
        }

        void set_scroll_sensitivity(float sensitivity)
        {
            scroll_sensitivity_ = sensitivity;
            std::println("[CameraController] Scroll sensitivity: {:.2f}", sensitivity);
        }

      private:
        void debug_print_input_events(glfw_input &input)
        {
            using namespace mcs::vulkan::event;

            static int frame_count = 0;
            frame_count++;

            // 每10帧打印一次输入状态
            if (frame_count % 10 != 0)
                return;

            // 检查鼠标事件
            const auto &mouse_left =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);
            const auto &mouse_right =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_RIGHT);
            const auto &mouse_middle =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_MIDDLE);

            if (mouse_left != event::mousebutton_event{})
            {
                std::println("[Input Debug] Frame {}: Mouse Left - press={}, release={}",
                             frame_count, mouse_left.press(), mouse_left.release());
            }
            if (mouse_right != event::mousebutton_event{})
            {
                std::println("[Input Debug] Frame {}: Mouse Right - press={}, release={}",
                             frame_count, mouse_right.press(), mouse_right.release());
            }
            if (mouse_middle != event::mousebutton_event{})
            {
                std::println(
                    "[Input Debug] Frame {}: Mouse Middle - press={}, release={}",
                    frame_count, mouse_middle.press(), mouse_middle.release());
            }

            // 检查鼠标位置
            auto cursor_pos = input.cursorPos();
            if (cursor_pos != event::position2d_event{})
            {
                std::println("[Input Debug] Frame {}: Cursor Pos=({:.1f}, {:.1f})",
                             frame_count, cursor_pos.xpos, cursor_pos.ypos);
            }
        }

        void handle_keyboard(glfw_input &input, float delta_time)
        {
            using namespace mcs::vulkan::event;

            const auto is_key_pressed = [&](Key key) {
                const auto &event = input.get_keyboard_event(key);
                return event.press() || event.repeat();
            };

            float speed = camera_->get_settings().movement_speed * delta_time;
            glm::vec3 movement(0.0f);

            // WASDQE 移动
            if (is_key_pressed(Key::W))
            {
                movement.z -= 1.0f;
                std::println("[CameraController] W pressed - move forward");
            }
            if (is_key_pressed(Key::S))
            {
                movement.z += 1.0f;
                std::println("[CameraController] S pressed - move backward");
            }
            if (is_key_pressed(Key::A))
            {
                movement.x -= 1.0f;
                std::println("[CameraController] A pressed - move left");
            }
            if (is_key_pressed(Key::D))
            {
                movement.x += 1.0f;
                std::println("[CameraController] D pressed - move right");
            }
            if (is_key_pressed(Key::Q))
            {
                movement.y -= 1.0f;
                std::println("[CameraController] Q pressed - move down");
            }
            if (is_key_pressed(Key::E))
            {
                movement.y += 1.0f;
                std::println("[CameraController] E pressed - move up");
            }

            // Shift加速
            if (is_key_pressed(Key::LEFT_SHIFT) || is_key_pressed(Key::RIGHT_SHIFT))
            {
                speed *= 2.0f;
                std::println("[CameraController] Shift pressed - speed boost");
            }

            // Ctrl减速
            if (is_key_pressed(Key::LEFT_CONTROL) || is_key_pressed(Key::RIGHT_CONTROL))
            {
                speed *= 0.5f;
                std::println("[CameraController] Ctrl pressed - slow mode");
            }

            // 执行移动
            if (glm::length(movement) > 0.0f)
            {
                movement = glm::normalize(movement);
                camera_->move(movement * speed, MovementAxis::Local);
            }

            // 模式切换快捷键
            if (is_key_pressed(Key::F1))
            {
                camera_->set_mode(CameraMode::FirstPerson);
                std::println("[CameraController] Switched to First Person mode");
            }
            if (is_key_pressed(Key::F2))
            {
                camera_->set_mode(CameraMode::Orbit);
                std::println("[CameraController] Switched to Orbit mode");
            }
            if (is_key_pressed(Key::F3))
            {
                camera_->set_mode(CameraMode::Free);
                std::println("[CameraController] Switched to Free mode");
            }

            // 重置相机
            if (is_key_pressed(Key::R))
            {
                camera_->reset();
                std::println("[CameraController] Camera reset");
            }
        }

        void handle_mouse(glfw_input &input, float delta_time)
        {
            using namespace mcs::vulkan::event;

            // 获取鼠标位置
            auto cursor_pos = input.cursorPos();
            if (cursor_pos == event::position2d_event{})
            {
                // 没有新的鼠标位置事件，但可能仍有按键需要处理
                process_mouse_buttons(input);
                return;
            }

            glm::vec2 current_pos(cursor_pos.xpos, cursor_pos.ypos);

            // 如果是第一次获取鼠标位置，初始化
            if (first_mouse_)
            {
                last_mouse_pos_ = current_pos;
                first_mouse_ = false;
                process_mouse_buttons(input); // 处理可能的按键事件
                return;
            }

            // 处理鼠标按键状态
            process_mouse_buttons(input);

            // 计算鼠标移动增量
            glm::vec2 delta = current_pos - last_mouse_pos_;

            // 只在有足够移动时才处理（避免抖动）
            if (glm::length(delta) < 0.1f)
            {
                // 仍然更新最后位置，但不处理移动
                last_mouse_pos_ = current_pos;
                return;
            }

            // 处理鼠标旋转（右键）
            if (mouse_rotating_)
            {
                float yaw = -delta.x * camera_->get_settings().rotation_speed *
                            mouse_sensitivity_;
                float pitch = -delta.y * camera_->get_settings().rotation_speed *
                              mouse_sensitivity_;

                // 应用旋转
                camera_->rotate(yaw, pitch);

                std::println("[CameraController] Rotating: delta({:.2f}, {:.2f}), "
                             "yaw={:.2f}, pitch={:.2f}",
                             delta.x, delta.y, yaw, pitch);

                last_mouse_pos_ = current_pos;
            }

            // 处理鼠标平移（中键）
            if (mouse_panning_)
            {
                glm::vec3 pan_offset(-delta.x * camera_->get_settings().pan_speed,
                                     delta.y * camera_->get_settings().pan_speed, 0.0f);

                // 应用平移
                camera_->move(pan_offset, MovementAxis::Screen);

                std::println("[CameraController] Panning: delta({:.2f}, {:.2f}), "
                             "offset({:.4f}, {:.4f}, {:.4f})",
                             delta.x, delta.y, pan_offset.x, pan_offset.y, pan_offset.z);

                last_mouse_pos_ = current_pos;
            }

            // 更新最后鼠标位置
            if (!mouse_rotating_ && !mouse_panning_)
            {
                last_mouse_pos_ = current_pos;
            }
        }

        void process_mouse_buttons(glfw_input &input)
        {
            using namespace mcs::vulkan::event;

            // 检查鼠标按键事件
            const auto &mouse_left_event =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);
            const auto &mouse_right_event =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_RIGHT);
            const auto &mouse_middle_event =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_MIDDLE);

            // 处理右键（旋转）
            if (mouse_right_event != event::mousebutton_event{})
            {
                if (mouse_right_event.press())
                {
                    mouse_rotating_ = true;
                    std::println("[CameraController] Right mouse button PRESSED - "
                                 "rotation enabled");
                }
                else if (mouse_right_event.release())
                {
                    mouse_rotating_ = false;
                    std::println("[CameraController] Right mouse button RELEASED - "
                                 "rotation disabled");
                }
            }

            // 处理中键（平移）
            if (mouse_middle_event != event::mousebutton_event{})
            {
                if (mouse_middle_event.press())
                {
                    mouse_panning_ = true;
                    std::println("[CameraController] Middle mouse button PRESSED - "
                                 "panning enabled");
                }
                else if (mouse_middle_event.release())
                {
                    mouse_panning_ = false;
                    std::println("[CameraController] Middle mouse button RELEASED - "
                                 "panning disabled");
                }
            }
        }

        void handle_scroll(glfw_input &input)
        {
            using namespace mcs::vulkan::event;

            auto scroll = input.scroll();
            if (scroll != event::scroll_event{})
            {
                float zoom_factor = scroll.yoffset * camera_->get_settings().zoom_speed *
                                    scroll_sensitivity_;

                camera_->move(glm::vec3(0, 0, zoom_factor), MovementAxis::Local);

                std::println(
                    "[CameraController] Scrolling: yoffset={:.1f}, zoom_factor={:.3f}",
                    scroll.yoffset, zoom_factor);
            }
        }

        std::string camera_mode_to_string(CameraMode mode)
        {
            switch (mode)
            {
            case CameraMode::FirstPerson:
                return "FirstPerson";
            case CameraMode::LookAt:
                return "LookAt";
            case CameraMode::Orbit:
                return "Orbit";
            case CameraMode::Free:
                return "Free";
            default:
                return "Unknown";
            }
        }

      private:
        std::shared_ptr<Camera> camera_;
        bool enabled_ = true;

        // 鼠标状态
        bool first_mouse_ = true;
        bool mouse_rotating_ = false;
        bool mouse_panning_ = false;
        glm::vec2 last_mouse_pos_ = glm::vec2(0.0f);

        // 灵敏度
        float mouse_sensitivity_ = 1.0f;
        float scroll_sensitivity_ = 1.0f;
    };

} // namespace mcs::vulkan::camera

using surface = mcs::vulkan::wsi::glfw::Window;
using glfw_input = mcs::vulkan::input::glfw_input;

// 修正测试函数
void test_camera_basic()
{
    using namespace mcs::vulkan::camera;

    auto camera = std::make_shared<Camera>();

    // 测试1: 基本设置
    camera->set_position(glm::vec3(0.0f, 0.0f, 5.0f));
    assert(camera->get_position() == glm::vec3(0.0f, 0.0f, 5.0f));

    // 测试2: 投影设置
    camera->set_perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    auto &settings = camera->get_settings();
    assert(settings.field_of_view == 60.0f);
    // 注意：现在相机内部存储宽高比，不再存储在settings中
    assert(camera->get_aspect_ratio() == 16.0f / 9.0f);

    // 测试3: 移动 - 修正：移动量就是偏移量本身，不乘以速度
    camera->move(glm::vec3(1.0f, 0.0f, 0.0f));
    auto pos = camera->get_position();
    // 位置从 (0,0,5) 移动到 (1,0,5)
    assert(pos.x == 1.0f);
    assert(pos.y == 0.0f);
    assert(pos.z == 5.0f);

    // 测试4: 获取变换矩阵
    auto view = camera->get_view_matrix();
    auto proj = camera->get_projection_matrix();
    auto vp = camera->get_view_projection_matrix();

    assert(glm::determinant(view) != 0.0f);
    assert(glm::determinant(proj) != 0.0f);

    std::println("[Test] 基础相机测试通过!");
}

void test_camera_modes()
{
    using namespace mcs::vulkan::camera;

    // 测试不同相机模式
    auto camera = std::make_shared<Camera>();

    // 第一人称模式
    camera->set_mode(CameraMode::FirstPerson);
    camera->set_position(glm::vec3(0.0f, 1.0f, 5.0f));
    camera->rotate(0.0f, -10.0f); // 向下看一点

    auto forward = camera->get_forward_vector();
    assert(forward.y < 0.0f); // 应该向下看

    // 轨道模式 - 修正：需要设置目标点
    camera->set_mode(CameraMode::Orbit);
    camera->set_position(glm::vec3(5.0f, 5.0f, 5.0f));
    camera->look_at(glm::vec3(0.0f, 0.0f, 0.0f));

    std::println("[Test] 相机模式测试通过!");
}

void test_camera_controller()
{
    using namespace mcs::vulkan::camera;

    auto camera = std::make_shared<Camera>();
    // 修正：set_perspective需要4个参数
    camera->set_perspective(45.0f, 1.0f, 0.1f, 100.0f);
    camera->set_position(glm::vec3(0.0f, 0.0f, 10.0f));

    auto controller = std::make_unique<CameraController>(camera);
    assert(controller->is_enabled());

    controller->set_enabled(false);
    assert(!controller->is_enabled());

    std::println("[Test] 相机控制器测试通过!");
}

// 添加一个简单的测试框架，用于检查相机功能
void run_camera_tests()
{
    std::println("=== 运行相机单元测试 ===");

    test_camera_basic();
    test_camera_modes();
    test_camera_controller();

    std::println("=== 所有单元测试通过 ===");
}

// 主测试程序
int main()
{
    std::println("=== 相机系统测试开始 ===");

    // 先运行单元测试
    try
    {
        run_camera_tests();
    }
    catch (const std::exception &e)
    {
        std::println("单元测试失败: {}", e.what());
        return 1;
    }

    // 集成测试
    try
    {
        surface window{};
        window.setup({.width = 800, .height = 600}, "Camera Test");

        glfw_input input;

        // 创建相机和控制器
        using namespace mcs::vulkan::camera;
        auto camera = std::make_shared<Camera>();
        auto controller = std::make_shared<CameraController>(camera);

        // 设置相机
        camera->set_perspective(45.0f, 800.0f / 600.0f, 0.1f, 100.0f);
        camera->set_position(glm::vec3(0.0f, 0.0f, 5.0f));
        camera->set_mode(CameraMode::FirstPerson);

        // 设置窗口大小
        auto [w, h] = window.getFramebufferSize();
        camera->set_window_size({w, h});

        auto last_time = std::chrono::high_resolution_clock::now();

        while (window.shouldClose() == 0)
        {
            auto current_time = std::chrono::high_resolution_clock::now();
            float delta_time =
                std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;

            // 重置和轮询事件
            input.reset();
            surface::pollEvents();

            // 打印当前帧信息
            static int frame = 0;
            frame++;
            // std::println("\n=== Frame {} (delta_time: {:.6f}s) ===", frame,
            // delta_time);

            // 更新相机控制器
            controller->update(input, delta_time);

            // 获取MVP矩阵用于渲染
            auto view_matrix = camera->get_view_matrix();
            auto proj_matrix = camera->get_projection_matrix();
            auto mvp_matrix = proj_matrix * view_matrix;

            // 打印相机状态（每秒一次）
            static float print_timer = 0.0f;
            print_timer += delta_time;
            if (print_timer > 1.0f)
            {
                auto pos = camera->get_position();
                auto forward = camera->get_forward_vector();
                auto yaw = camera->get_yaw();
                auto pitch = camera->get_pitch();

                std::println("[Camera] Pos({:.2f}, {:.2f}, {:.2f}), "
                             "Yaw/Pitch({:.1f}, {:.1f}), "
                             "Forward({:.2f}, {:.2f}, {:.2f})",
                             pos.x, pos.y, pos.z, yaw, pitch, forward.x, forward.y,
                             forward.z);
                print_timer = 0.0f;
            }

            if (window.framebufferResized())
            {
                auto [w, h] = window.getFramebufferSize();
                camera->set_window_size({w, h});
                window.refFramebufferResized() = false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        window.teardown();
    }
    catch (const std::exception &e)
    {
        std::println("集成测试错误: {}", e.what());
        return 1;
    }

    std::println("=== 相机系统测试完成 ===");
    return 0;
}
/*
=== 相机系统测试开始 ===
=== 运行相机单元测试 ===
[Camera] set_perspective: Perspective projection, FOV=45.0, Aspect=1.78, Near=0.10,
Far=100.00 [Camera] set_position: Position(0.00, 0.00, 5.00), Forward(0.00, 0.00, -1.00)
[Camera] set_perspective: Perspective projection, FOV=60.0, Aspect=1.78, Near=0.10,
Far=100.00 [Test] 基础相机测试通过! [Camera] set_perspective: Perspective projection,
FOV=45.0, Aspect=1.78, Near=0.10, Far=100.00 [Camera] set_position:
Position(0.00, 1.00, 5.00), Forward(0.00, 0.00, -1.00) [Camera] set_position:
Position(5.00, 5.00, 5.00), Forward(0.00, -0.17, -0.98) [Test] 相机模式测试通过! [Camera]
set_perspective: Perspective projection, FOV=45.0, Aspect=1.78, Near=0.10, Far=100.00
[Camera] set_perspective: Perspective projection, FOV=45.0, Aspect=1.00, Near=0.10,
Far=100.00 [Camera] set_position: Position(0.00, 0.00, 10.00), Forward(0.00, 0.00, -1.00)
[CameraController] Created with camera mode: FirstPerson
[CameraController] Disabled
[Test] 相机控制器测试通过!
=== 所有单元测试通过 ===
[Camera] set_perspective: Perspective projection, FOV=45.0, Aspect=1.78, Near=0.10,
Far=100.00 [CameraController] Created with camera mode: FirstPerson [Camera]
set_perspective: Perspective projection, FOV=45.0, Aspect=1.33, Near=0.10, Far=100.00
[Camera] set_position: Position(0.00, 0.00, 5.00), Forward(0.00, 0.00, -1.00)
cursorEnter: cursor_enter_event{entered=true}
[CameraController] Q pressed - move down
[CameraController] W pressed - move forward
[CameraController] E pressed - move up
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(435.0, 177.0)
[Camera] Pos(0.00, -0.00, 4.92), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
[CameraController] A pressed - move left
[CameraController] S pressed - move backward
[CameraController] D pressed - move right
[CameraController] A pressed - move left
[CameraController] E pressed - move up
[CameraController] Q pressed - move down
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(435.0, 177.0)
[Camera] Pos(-0.09, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
[Camera] Camera reset
[CameraController] Camera reset
[CameraController] A pressed - move left
[CameraController] D pressed - move right
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(435.0, 177.0)
[Camera] Pos(0.00, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(435.0, 177.0)
[Camera] Pos(0.00, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
[CameraController] Ctrl pressed - slow mode
[CameraController] Ctrl pressed - slow mode
mouse: mousebutton_event{button=MOUSE_BUTTON_LEFT, action=PRESS, modifier=NONE}
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(435.0, 177.0)
[Camera] Pos(0.00, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
mouse: mousebutton_event{button=MOUSE_BUTTON_LEFT, action=RELEASE, modifier=NONE}
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(435.0, 177.0)
[Camera] Pos(0.00, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
mouse: mousebutton_event{button=MOUSE_BUTTON_LEFT, action=PRESS, modifier=NONE}
[Input Debug] Frame 360: Mouse Left - press=true, release=false
[Input Debug] Frame 380: Cursor Pos=(402.0, 177.0)
[Input Debug] Frame 390: Cursor Pos=(359.0, 182.0)
[Input Debug] Frame 400: Cursor Pos=(326.0, 188.0)
[Input Debug] Frame 410: Cursor Pos=(292.0, 193.0)
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(272.0, 198.0)
[Camera] Pos(0.00, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
[Input Debug] Frame 420: Cursor Pos=(261.0, 201.0)
[Input Debug] Frame 440: Cursor Pos=(269.0, 201.0)
[Input Debug] Frame 450: Cursor Pos=(307.0, 202.0)
[Input Debug] Frame 460: Cursor Pos=(347.0, 203.0)
[Input Debug] Frame 470: Cursor Pos=(365.0, 203.0)
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(374.0, 203.0)
[Camera] Pos(0.00, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
mouse: mousebutton_event{button=MOUSE_BUTTON_LEFT, action=RELEASE, modifier=NONE}
mouse: mousebutton_event{button=MOUSE_BUTTON_MIDDLE, action=PRESS, modifier=NONE}
[Input Debug] Frame 530: Mouse Middle - press=true, release=false
[CameraController] Middle mouse button PRESSED - panning enabled
[CameraController] Status: rotating=false, panning=true, last_mouse_pos=(376.0, 203.0)
[Camera] Pos(0.00, 0.00, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
[CameraController] Panning: delta(0.00, 1.00), offset(-0.0000, 0.0050, 0.0000)
[CameraController] Panning: delta(2.00, 0.00), offset(-0.0100, 0.0000, 0.0000)
[CameraController] Panning: delta(0.00, 1.00), offset(-0.0000, 0.0050, 0.0000)
[CameraController] Panning: delta(-1.00, 1.00), offset(0.0050, 0.0050, 0.0000)
[CameraController] Panning: delta(1.00, 1.00), offset(-0.0050, 0.0050, 0.0000)
[CameraController] Panning: delta(1.00, 0.00), offset(-0.0050, 0.0000, 0.0000)
[Input Debug] Frame 570: Cursor Pos=(381.0, 207.0)
[CameraController] Panning: delta(2.00, 0.00), offset(-0.0100, 0.0000, 0.0000)
[CameraController] Panning: delta(2.00, 0.00), offset(-0.0100, 0.0000, 0.0000)
[CameraController] Panning: delta(3.00, 2.00), offset(-0.0150, 0.0100, 0.0000)
[CameraController] Panning: delta(2.00, 0.00), offset(-0.0100, 0.0000, 0.0000)
[CameraController] Panning: delta(4.00, 0.00), offset(-0.0200, 0.0000, 0.0000)
[CameraController] Panning: delta(4.00, 0.00), offset(-0.0200, 0.0000, 0.0000)
[CameraController] Panning: delta(4.00, 0.00), offset(-0.0200, 0.0000, 0.0000)
[CameraController] Panning: delta(5.00, 0.00), offset(-0.0250, 0.0000, 0.0000)
[CameraController] Panning: delta(7.00, 0.00), offset(-0.0350, 0.0000, 0.0000)
[CameraController] Panning: delta(4.00, 0.00), offset(-0.0200, 0.0000, 0.0000)
[Input Debug] Frame 580: Cursor Pos=(420.0, 209.0)
[CameraController] Panning: delta(4.00, 0.00), offset(-0.0200, 0.0000, 0.0000)
[CameraController] Panning: delta(4.00, 0.00), offset(-0.0200, 0.0000, 0.0000)
[CameraController] Panning: delta(5.00, 0.00), offset(-0.0250, 0.0000, 0.0000)
[CameraController] Panning: delta(4.00, 0.00), offset(-0.0200, 0.0000, 0.0000)
[CameraController] Panning: delta(2.00, 0.00), offset(-0.0100, 0.0000, 0.0000)
[CameraController] Panning: delta(1.00, 0.00), offset(-0.0050, 0.0000, 0.0000)
[CameraController] Panning: delta(2.00, 0.00), offset(-0.0100, 0.0000, 0.0000)
[CameraController] Panning: delta(2.00, 0.00), offset(-0.0100, 0.0000, 0.0000)
[CameraController] Panning: delta(1.00, 0.00), offset(-0.0050, 0.0000, 0.0000)
[CameraController] Status: rotating=false, panning=true, last_mouse_pos=(441.0, 209.0)
[Camera] Pos(-0.32, 0.03, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
mouse: mousebutton_event{button=MOUSE_BUTTON_MIDDLE, action=RELEASE, modifier=NONE}
[CameraController] Middle mouse button RELEASED - panning disabled
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(446.0, 207.0)
[Camera] Pos(-0.32, 0.03, 5.00), Yaw/Pitch(0.0, 0.0), Forward(0.00, 0.00, -1.00)
mouse: mousebutton_event{button=MOUSE_BUTTON_RIGHT, action=PRESS, modifier=NONE}
[CameraController] Right mouse button PRESSED - rotation enabled
[CameraController] Rotating: delta(1.00, 0.00), yaw=-0.25, pitch=-0.00
[CameraController] Rotating: delta(1.00, 0.00), yaw=-0.25, pitch=-0.00
[Input Debug] Frame 670: Cursor Pos=(452.0, 206.0)
[CameraController] Rotating: delta(4.00, -1.00), yaw=-1.00, pitch=0.25
[CameraController] Rotating: delta(7.00, 0.00), yaw=-1.75, pitch=-0.00
[CameraController] Rotating: delta(6.00, -1.00), yaw=-1.50, pitch=0.25
[CameraController] Rotating: delta(7.00, -2.00), yaw=-1.75, pitch=0.50
[CameraController] Rotating: delta(11.00, 0.00), yaw=-2.75, pitch=-0.00
[CameraController] Rotating: delta(6.00, 0.00), yaw=-1.50, pitch=-0.00
[CameraController] Rotating: delta(6.00, 0.00), yaw=-1.50, pitch=-0.00
[CameraController] Rotating: delta(4.00, 0.00), yaw=-1.00, pitch=-0.00
[CameraController] Rotating: delta(7.00, 0.00), yaw=-1.75, pitch=-0.00
[CameraController] Rotating: delta(7.00, 0.00), yaw=-1.75, pitch=-0.00
[Input Debug] Frame 680: Cursor Pos=(517.0, 203.0)
[CameraController] Rotating: delta(4.00, 0.00), yaw=-1.00, pitch=-0.00
[CameraController] Rotating: delta(2.00, 0.00), yaw=-0.50, pitch=-0.00
[CameraController] Status: rotating=true, panning=false, last_mouse_pos=(519.0, 203.0)
[Camera] Pos(-0.32, 0.03, 5.00), Yaw/Pitch(-18.2, 1.0), Forward(0.31, 0.02, -0.95)
mouse: mousebutton_event{button=MOUSE_BUTTON_RIGHT, action=RELEASE, modifier=NONE}
[Input Debug] Frame 720: Mouse Right - press=false, release=true
[CameraController] Right mouse button RELEASED - rotation disabled
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(519.0, 203.0)
[Camera] Pos(-0.32, 0.03, 5.00), Yaw/Pitch(-18.2, 1.0), Forward(0.31, 0.02, -0.95)
[CameraController] Scrolling: yoffset=-1.0, zoom_factor=-1.000
[CameraController] Scrolling: yoffset=-1.0, zoom_factor=-1.000
[CameraController] Scrolling: yoffset=-1.0, zoom_factor=-1.000
[CameraController] Scrolling: yoffset=-1.0, zoom_factor=-1.000
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(519.0, 203.0)
[Camera] Pos(0.93, 0.10, 1.20), Yaw/Pitch(-18.2, 1.0), Forward(0.31, 0.02, -0.95)
[CameraController] Scrolling: yoffset=1.0, zoom_factor=1.000
[CameraController] Scrolling: yoffset=1.0, zoom_factor=1.000
[CameraController] Scrolling: yoffset=1.0, zoom_factor=1.000
[CameraController] Scrolling: yoffset=1.0, zoom_factor=1.000
[CameraController] Status: rotating=false, panning=false, last_mouse_pos=(519.0, 203.0)
[Camera] Pos(-0.32, 0.03, 5.00), Yaw/Pitch(-18.2, 1.0), Forward(0.31, 0.02, -0.95)
[ DEBUG ] [glfw.hpp:237:static void __cdecl
mcs::vulkan::wsi::glfw::Window::keyCallback(GLFWwindow *, int, int, int, int)]: 按下了 ESC
键，退出程序
=== 相机系统测试完成 ===
*/
// NOLINTEND