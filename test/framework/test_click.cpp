#include "./head.hpp"
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <print>
#include <utility>
#include <chrono>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_set>

#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

using surface = mcs::vulkan::wsi::glfw::Window;
using glfw_input = mcs::vulkan::input::glfw_input;
using Key = mcs::vulkan::event::Key;
using ModifierKey = mcs::vulkan::event::ModifierKey;
using MouseButtons = mcs::vulkan::event::MouseButtons;
using Action = mcs::vulkan::event::Action;

// 相机系统，提供第一人称/观察者相机功能
namespace mcs::vulkan::camera
{
    // 窗口尺寸结构体，包含宽高和宽高比计算
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

    // 相机类：管理相机位置、朝向、投影矩阵
    class Camera
    {
      public:
        Camera()
        {
            reset();
        }

        // 重置相机到默认状态
        void reset()
        {
            position_ = glm::vec3(0.0f, 0.0f, 5.0f);
            yaw_ = 0.0f;
            pitch_ = 0.0f;
            roll_ = 0.0f;
            orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            update_orientation_from_euler();

            set_perspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        }

        // 设置透视投影参数
        void set_perspective(float fov, float aspect, float near, float far)
        {
            fov_ = fov;
            aspect_ratio_ = aspect;
            near_ = near;
            far_ = far;
            projection_matrix_ =
                glm::perspective(glm::radians(fov_), aspect_ratio_, near_, far_);
        }

        // 设置相机位置
        void set_position(const glm::vec3 &position)
        {
            position_ = position;
            update_view_matrix();
        }

        // 沿着相机朝向移动
        void move(const glm::vec3 &offset)
        {
            position_ += orientation_ * offset;
            update_view_matrix();
        }

        // 屏幕平面平移（沿相机右方向和上方向）
        void pan(float right_offset, float up_offset)
        {
            position_ += get_right_vector() * right_offset + get_up_vector() * up_offset;
            update_view_matrix();
        }

        // 旋转相机（偏航和俯仰）
        void rotate(float yaw, float pitch)
        {
            yaw_ += yaw;
            pitch_ += pitch;
            pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
            update_orientation_from_euler();
            update_view_matrix();
        }

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
        [[nodiscard]] const glm::mat4 &get_view_matrix() const noexcept
        {
            return view_matrix_;
        }
        [[nodiscard]] const glm::mat4 &get_projection_matrix() const noexcept
        {
            return projection_matrix_;
        }
        [[nodiscard]] float get_yaw() const noexcept
        {
            return yaw_;
        }
        [[nodiscard]] float get_pitch() const noexcept
        {
            return pitch_;
        }

        // 更新窗口尺寸（重新计算投影矩阵）
        void set_window_size(const WindowSize &size)
        {
            if (size.is_valid())
            {
                set_perspective(fov_, size.aspect_ratio(), near_, far_);
            }
        }

      private:
        // 从欧拉角更新四元数朝向
        void update_orientation_from_euler()
        {
            float yaw_rad = glm::radians(yaw_);
            float pitch_rad = glm::radians(pitch_);
            float roll_rad = glm::radians(roll_);

            glm::quat q_yaw = glm::angleAxis(yaw_rad, glm::vec3(0, 1, 0));
            glm::quat q_pitch = glm::angleAxis(pitch_rad, glm::vec3(1, 0, 0));
            glm::quat q_roll = glm::angleAxis(roll_rad, glm::vec3(0, 0, 1));

            orientation_ = q_yaw * q_pitch * q_roll;
            orientation_ = glm::normalize(orientation_);
        }

        // 更新视图矩阵
        void update_view_matrix()
        {
            view_matrix_ =
                glm::lookAt(position_, position_ + get_forward_vector(), get_up_vector());
        }

        glm::vec3 position_;
        glm::quat orientation_;
        float yaw_ = 0.0f;   // 偏航角
        float pitch_ = 0.0f; // 俯仰角
        float roll_ = 0.0f;  // 滚转角

        glm::mat4 view_matrix_;
        glm::mat4 projection_matrix_;

        float fov_;
        float aspect_ratio_;
        float near_;
        float far_;
    };

    // 相机控制器：处理用户输入控制相机
    class CameraController
    {
      public:
        explicit CameraController(std::shared_ptr<Camera> camera)
            : camera_(std::move(camera))
        {
        }

        // 每帧更新：处理键盘、鼠标、滚轮输入
        void update(glfw_input &input, float delta_time)
        {
            if (!camera_ || !enabled_)
                return;

            handle_keyboard(input, delta_time);
            handle_mouse(input);
            handle_scroll(input);
        }

        void set_enabled(bool enabled)
        {
            enabled_ = enabled;
        }

        [[nodiscard]] glm::quat get_model_rotation() const noexcept
        {
            return model_rotation_;
        }
        void reset_model_rotation()
        {
            model_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }

      private:
        // 处理键盘输入：WASD移动，Ctrl减速，R重置相机，T重置模型旋转
        void handle_keyboard(glfw_input &input, float delta_time)
        {
            using namespace mcs::vulkan::event;
            const auto is_key_pressed = [&](Key key) {
                const auto &event = input.get_keyboard_event(key);
                return event.press() || event.repeat();
            };

            float speed = 5.0f * delta_time;
            glm::vec3 movement(0.0f);
            bool moved = false;

            if (is_key_pressed(Key::W))
            {
                movement.z -= 1.0f;
                moved = true;
            }
            if (is_key_pressed(Key::S))
            {
                movement.z += 1.0f;
                moved = true;
            }
            if (is_key_pressed(Key::A))
            {
                movement.x += 1.0f;
                moved = true;
            }
            if (is_key_pressed(Key::D))
            {
                movement.x -= 1.0f;
                moved = true;
            }
            if (is_key_pressed(Key::Q))
            {
                movement.y += 1.0f;
                moved = true;
            }
            if (is_key_pressed(Key::E))
            {
                movement.y -= 1.0f;
                moved = true;
            }

            if (is_key_pressed(Key::LEFT_CONTROL) || is_key_pressed(Key::RIGHT_CONTROL))
                speed *= 0.5f;

            if (moved)
            {
                movement = glm::normalize(movement);
                camera_->move(movement * speed);
            }

            if (is_key_pressed(Key::R))
                camera_->reset();
            if (is_key_pressed(Key::T))
                reset_model_rotation();
        }

        // 处理鼠标输入：左键旋转模型，右键旋转相机，中键平移
        void handle_mouse(glfw_input &input)
        {
            using namespace mcs::vulkan::event;

            const auto &mouse_left =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);
            auto cursor_pos = input.cursorPos();
            if (cursor_pos == event::position2d_event{})
                return;

            glm::vec2 current_pos(cursor_pos.xpos, cursor_pos.ypos);
            glm::vec2 delta = current_pos - last_mouse_pos_;
            last_mouse_pos_ = current_pos;

            const auto is_key_pressed = [&](Key key) {
                const auto &event = input.get_keyboard_event(key);
                return event.press() || event.repeat();
            };
            const auto CTRL_PRESSED =
                is_key_pressed(Key::LEFT_CONTROL) || is_key_pressed(Key::RIGHT_CONTROL);

            float sensitivity = 0.3f;

            if (CTRL_PRESSED && mouse_left.press())
            {
                // Ctrl+左键：绕Z轴旋转模型
                float delta_angle = delta.x * sensitivity * 0.5f;
                glm::quat rotation = glm::angleAxis(glm::radians(delta_angle),
                                                    glm::vec3(0.0f, 0.0f, 1.0f));
                model_rotation_ = rotation * model_rotation_;
                model_rotation_ = glm::normalize(model_rotation_);
            }
            else if (mouse_left.press())
            {
                // 左键：绕X和Y轴旋转模型
                float pitch_angle = -delta.y * sensitivity;
                float yaw_angle = delta.x * sensitivity;
                glm::quat pitch_rotation = glm::angleAxis(glm::radians(pitch_angle),
                                                          glm::vec3(1.0f, 0.0f, 0.0f));
                glm::quat yaw_rotation =
                    glm::angleAxis(glm::radians(yaw_angle), glm::vec3(0.0f, 1.0f, 0.0f));
                model_rotation_ = yaw_rotation * pitch_rotation * model_rotation_;
                model_rotation_ = glm::normalize(model_rotation_);
            }

            const auto &mouse_right =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_RIGHT);
            if (mouse_right.press() && glm::length(delta) > 0.0f)
            {
                // 右键：旋转相机
                float sensitivity = 0.1f;
                float yaw = -delta.x * sensitivity;
                float pitch = delta.y * sensitivity;
                camera_->rotate(yaw, pitch);
            }

            const auto &mouse_middle =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_MIDDLE);
            if (mouse_middle.press() && glm::length(delta) > 0.0f)
            {
                // 中键：平移相机
                float pan_speed = 0.005f;
                float right_offset = -delta.x * pan_speed;
                float up_offset = delta.y * pan_speed;
                camera_->pan(right_offset, up_offset);
            }
        }

        // 处理滚轮输入：缩放
        void handle_scroll(glfw_input &input)
        {
            using namespace mcs::vulkan::event;
            const auto &scroll = input.scroll();
            if (scroll != event::scroll_event{})
            {
                float zoom_factor = scroll.yoffset * 0.5f;
                camera_->move(glm::vec3(0, 0, zoom_factor));
            }
        }

      private:
        std::shared_ptr<Camera> camera_;
        bool enabled_ = true;
        glm::vec2 last_mouse_pos_ = glm::vec2(0.0f);
        glm::quat model_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // 模型旋转
    };
} // namespace mcs::vulkan::camera

// 类型别名定义
using make_physical_device = mcs::vulkan::make_physical_device;
using make_instance = mcs::vulkan::make_instance;
using physical_device = mcs::vulkan::physical_device;
using make_logical_device = mcs::vulkan::make_logical_device;
using make_queue_family_index = mcs::vulkan::make_queue_family_index;
using logical_device = mcs::vulkan::logical_device;
using mcs::vulkan::structure_chain;
using mcs::vulkan::sType;
using mcs::vulkan::choose_swap_surface_format;
using mcs::vulkan::choose_swap_present_mode;

// 帧上下文：管理每一帧的同步对象（信号量、围栏）
struct FrameContext
{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    const logical_device *device_{};
    std::vector<VkSemaphore> presentCompleteSemaphore; // 表示图像可用于呈现的信号量
    std::vector<VkSemaphore> renderFinishedSemaphore;  // 表示渲染完成的信号量
    std::vector<VkFence> inFlightFences;               // 确保帧不会超前的围栏
    uint32_t semaphoreIndex = 0;                       // 当前使用的信号量索引
    uint32_t currentFrame = 0;                         // 当前帧索引

    explicit FrameContext(const logical_device &device, size_t swapChainImagesSize)
        : device_{&device}
    {
        createSyncObjects(device.raw_data(), swapChainImagesSize);
    }

    ~FrameContext() noexcept
    {
        destroy();
    }
    FrameContext(const FrameContext &) = delete;
    FrameContext(FrameContext &&) = delete;
    FrameContext &operator=(const FrameContext &) = delete;
    FrameContext &operator=(FrameContext &&) = delete;

  private:
    // 创建同步对象
    void createSyncObjects(const VkDevice &device, size_t swapChainImagesSize)
    {
        destroySyncObject();

        presentCompleteSemaphore.resize(swapChainImagesSize);
        renderFinishedSemaphore.resize(swapChainImagesSize);
        VkSemaphoreCreateInfo semaphoreInfo = {.sType = sType<VkSemaphoreCreateInfo>()};

        for (size_t i = 0; i < swapChainImagesSize; i++)
        {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                  &presentCompleteSemaphore[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                  &renderFinishedSemaphore[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphores!");
        }

        VkFenceCreateInfo fenceInfo = {.sType = sType<VkFenceCreateInfo>(),
                                       .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create fence!");
        }
    }

    // 销毁同步对象
    void destroySyncObject() noexcept
    {
        if (device_ != nullptr)
        {
            for (auto &semaphore : presentCompleteSemaphore)
                vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
            for (auto &semaphore : renderFinishedSemaphore)
                vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
            for (auto &fence : inFlightFences)
                vkDestroyFence(device_->raw_data(), fence, nullptr);
        }
        presentCompleteSemaphore.clear();
        renderFinishedSemaphore.clear();
        inFlightFences.clear();
    }

    void destroy() noexcept
    {
        if (device_ != nullptr)
        {
            destroySyncObject();
            device_ = nullptr;
        }
    }
};

// 渲染辅助函数
struct render
{
    // 图像布局转换辅助函数
    static void transition_image_layout(VkCommandBuffer commandBuffer, VkImage image,
                                        VkImageAspectFlags aspectMask,
                                        VkImageLayout oldLayout, VkImageLayout newLayout,
                                        VkAccessFlags srcAccessMask,
                                        VkAccessFlags dstAccessMask,
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask)
    {
        VkImageMemoryBarrier2 barrier = {.sType = sType<VkImageMemoryBarrier2>(),
                                         .srcStageMask = srcStageMask,
                                         .srcAccessMask = srcAccessMask,
                                         .dstStageMask = dstStageMask,
                                         .dstAccessMask = dstAccessMask,
                                         .oldLayout = oldLayout,
                                         .newLayout = newLayout,
                                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .image = image,
                                         .subresourceRange = {.aspectMask = aspectMask,
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1}};
        VkDependencyInfo dependency_info = {.sType = sType<VkDependencyInfo>(),
                                            .dependencyFlags = {},
                                            .imageMemoryBarrierCount = 1,
                                            .pImageMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(commandBuffer, &dependency_info);
    }
};

// 常量定义
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "Scene Test with Color Picking and Click";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr auto VERT_SHADER_PATH = "shaders/test_click_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_select_frag.spv";
constexpr auto PICKING_VERT_SHADER_PATH = "shaders/picking_vert.spv";
constexpr auto PICKING_FRAG_SHADER_PATH = "shaders/picking_frag.spv";

// 相机统一缓冲区对象（UBO）结构体
struct alignas(16) CameraUBO
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Vulkan API版本号生成器
template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

// 顶点数据结构
class Vertex
{
  public:
    glm::vec3 pos;
    glm::vec3 color;

    // 将GLM向量类型映射到Vulkan格式
    template <typename T>
    static consteval VkFormat mappFormat()
    {
        if constexpr (std::same_as<glm::vec1, T>)
            return VK_FORMAT_R32_SFLOAT;
        else if constexpr (std::same_as<glm::vec2, T>)
            return VK_FORMAT_R32G32_SFLOAT;
        else if constexpr (std::same_as<glm::vec3, T>)
            return VK_FORMAT_R32G32B32_SFLOAT;
        else if constexpr (std::same_as<glm::vec4, T>)
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        else
            throw;
    }

    // 获取顶点绑定描述
    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    // 获取顶点属性描述（用于常规渲染）
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {VkVertexInputAttributeDescription{.location = 0,
                                                  .binding = 0,
                                                  .format =
                                                      mappFormat<decltype(Vertex::pos)>(),
                                                  .offset = offsetof(Vertex, pos)},
                VkVertexInputAttributeDescription{
                    .location = 1,
                    .binding = 0,
                    .format = mappFormat<decltype(Vertex::color)>(),
                    .offset = offsetof(Vertex, color)}};
    }

    // 获取拾取渲染的顶点属性描述（只包含位置）
    static std::array<VkVertexInputAttributeDescription, 1>
    getPickingAttributeDescriptions()
    {
        return {VkVertexInputAttributeDescription{.location = 0,
                                                  .binding = 0,
                                                  .format =
                                                      mappFormat<decltype(Vertex::pos)>(),
                                                  .offset = offsetof(Vertex, pos)}};
    }
};

// 几何体系统：定义不同类型的几何体（三角形、平面、立方体）
namespace mcs::vulkan::geometry
{
    class Geometry
    {
      public:
        virtual ~Geometry() = default;
        virtual std::vector<Vertex> getVertices() const = 0;
        virtual std::vector<uint32_t> getIndices() const = 0;
        [[nodiscard]] virtual std::string getName() const
        {
            return "Geometry";
        }
    };

    // 三角形几何体
    class TriangleGeometry : public Geometry
    {
      public:
        TriangleGeometry() = default;
        std::vector<Vertex> getVertices() const override
        {
            return {{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
        }
        std::vector<uint32_t> getIndices() const override
        {
            return {0, 1, 2};
        }
        [[nodiscard]] std::string getName() const override
        {
            return "Triangle";
        }
    };

    // 平面几何体
    class PlaneGeometry : public Geometry
    {
      public:
        PlaneGeometry(float width = 1.0f, float height = 1.0f)
            : width_(width), height_(height)
        {
        }

        std::vector<Vertex> getVertices() const override
        {
            float halfWidth = width_ * 0.5f;
            float halfHeight = height_ * 0.5f;
            return {{{-halfWidth, 0.0f, -halfHeight}, {0.8f, 0.8f, 0.8f}},
                    {{halfWidth, 0.0f, -halfHeight}, {0.8f, 0.8f, 0.8f}},
                    {{halfWidth, 0.0f, halfHeight}, {0.8f, 0.8f, 0.8f}},
                    {{-halfWidth, 0.0f, halfHeight}, {0.8f, 0.8f, 0.8f}}};
        }
        std::vector<uint32_t> getIndices() const override
        {
            return {0, 1, 2, 2, 3, 0};
        }
        [[nodiscard]] std::string getName() const override
        {
            return "Plane(" + std::to_string(width_) + "x" + std::to_string(height_) +
                   ")";
        }

      private:
        float width_;
        float height_;
    };

    // 立方体几何体
    class CubeGeometry : public Geometry
    {
      public:
        CubeGeometry(float size = 1.0f) : size_(size) {}
        std::vector<Vertex> getVertices() const override
        {
            float half = size_ * 0.5f;
            return {// Front
                    {{-half, -half, half}, {1.0f, 0.0f, 0.0f}},
                    {{half, -half, half}, {0.0f, 1.0f, 0.0f}},
                    {{half, half, half}, {0.0f, 0.0f, 1.0f}},
                    {{-half, half, half}, {1.0f, 1.0f, 1.0f}},
                    // Back
                    {{-half, -half, -half}, {1.0f, 0.0f, 0.0f}},
                    {{half, -half, -half}, {0.0f, 1.0f, 0.0f}},
                    {{half, half, -half}, {0.0f, 0.0f, 1.0f}},
                    {{-half, half, -half}, {1.0f, 1.0f, 1.0f}}};
        }
        std::vector<uint32_t> getIndices() const override
        {
            return {
                0, 1, 2, 2, 3, 0, // Front
                4, 5, 6, 6, 7, 4, // Back
                0, 3, 7, 7, 4, 0, // Left
                1, 2, 6, 6, 5, 1, // Right
                3, 2, 6, 6, 7, 3, // Top
                0, 1, 5, 5, 4, 0  // Bottom
            };
        }
        [[nodiscard]] std::string getName() const override
        {
            return "Cube(" + std::to_string(size_) + ")";
        }

      private:
        float size_;
    };
} // namespace mcs::vulkan::geometry

// 场景图系统：管理3D对象
namespace mcs::vulkan::core
{
    // 3D对象类：包含几何体、变换、可见性等属性
    class Object3D
    {
      public:
        Object3D(std::shared_ptr<geometry::Geometry> geometry, uint32_t objectId)
            : geometry_(std::move(geometry)), objectId_(objectId), position_(0.0f),
              rotation_(1.0f, 0.0f, 0.0f, 0.0f), scale_(1.0f), visible_(true),
              name_("Object3D"), hovered_(false), selected_(false), clickAnimTime_(0.0f)
        {
        }

        virtual ~Object3D() = default;

        void setPosition(const glm::vec3 &position)
        {
            position_ = position;
        }
        void setRotation(const glm::quat &rotation)
        {
            rotation_ = rotation;
        }
        void setScale(const glm::vec3 &scale)
        {
            scale_ = scale;
        }
        void setScale(float scale)
        {
            scale_ = glm::vec3(scale);
        }
        void setVisible(bool visible)
        {
            visible_ = visible;
        }
        void setName(const std::string &name)
        {
            name_ = name;
        }
        void setHovered(bool hovered)
        {
            hovered_ = hovered;
        }
        void setSelected(bool selected)
        {
            selected_ = selected;
            if (selected)
            {
                // 开始点击动画
                clickAnimTime_ = 1.0f;
                originalScale_ = scale_;
            }
        }
        void update(float deltaTime)
        {
            if (clickAnimTime_ > 0.0f)
            {
                clickAnimTime_ -= deltaTime * 5.0f; // 动画速度
                if (clickAnimTime_ < 0.0f)
                    clickAnimTime_ = 0.0f;

                // 脉冲效果：先放大再恢复
                float pulse =
                    1.0f + sinf(clickAnimTime_ * glm::pi<float>() * 2.0f) * 0.2f;
                scale_ = originalScale_ * pulse;
            }
        }
        [[nodiscard]] bool isHovered() const
        {
            return hovered_;
        }
        [[nodiscard]] bool isSelected() const
        {
            return selected_;
        }
        [[nodiscard]] uint32_t getObjectId() const
        {
            return objectId_;
        }
        [[nodiscard]] const glm::vec3 &getPosition() const
        {
            return position_;
        }
        [[nodiscard]] const glm::quat &getRotation() const
        {
            return rotation_;
        }
        [[nodiscard]] const glm::vec3 &getScale() const
        {
            return scale_;
        }
        [[nodiscard]] bool isVisible() const
        {
            return visible_;
        }
        [[nodiscard]] const std::string &getName() const
        {
            return name_;
        }
        [[nodiscard]] std::shared_ptr<geometry::Geometry> getGeometry() const
        {
            return geometry_;
        }
        [[nodiscard]] float getClickAnimTime() const
        {
            return clickAnimTime_;
        }

        void translate(const glm::vec3 &offset)
        {
            position_ += offset;
        }
        void rotate(float angle, const glm::vec3 &axis)
        {
            rotation_ = glm::angleAxis(glm::radians(angle), axis) * rotation_;
        }
        void rotateX(float angle)
        {
            rotate(angle, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        void rotateY(float angle)
        {
            rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        void rotateZ(float angle)
        {
            rotate(angle, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        // 获取模型变换矩阵
        [[nodiscard]] glm::mat4 getModelMatrix() const
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, position_);
            model = model * glm::mat4_cast(rotation_);
            model = glm::scale(model, scale_);
            return model;
        }

      private:
        std::shared_ptr<geometry::Geometry> geometry_;
        uint32_t objectId_; // 用于颜色拾取的对象ID
        glm::vec3 position_;
        glm::quat rotation_;
        glm::vec3 scale_;
        glm::vec3 originalScale_; // 原始缩放值
        bool visible_;
        std::string name_;
        bool hovered_;        // 鼠标悬停状态
        bool selected_;       // 鼠标选中状态
        float clickAnimTime_; // 点击动画时间
    };
} // namespace mcs::vulkan::core

// 场景类：管理所有3D对象
namespace mcs::vulkan::core
{
    class Scene
    {
      public:
        Scene() : nextObjectId_(1) {} // 对象ID从1开始（0表示无对象）

        void add(std::shared_ptr<Object3D> object)
        {
            objects_.push_back(std::move(object));
        }

        void add(const std::vector<std::shared_ptr<Object3D>> &objects)
        {
            for (const auto &obj : objects)
            {
                add(obj);
            }
        }

        void remove(std::shared_ptr<Object3D> object)
        {
            auto it = std::find(objects_.begin(), objects_.end(), object);
            if (it != objects_.end())
            {
                objects_.erase(it);
            }
        }

        void clear()
        {
            objects_.clear();
            nextObjectId_ = 1;
        }

        void update(float deltaTime)
        {
            for (auto &obj : objects_)
            {
                obj->update(deltaTime);
            }
        }

        [[nodiscard]] const std::vector<std::shared_ptr<Object3D>> &getObjects() const
        {
            return objects_;
        }
        [[nodiscard]] std::vector<std::shared_ptr<Object3D>> &getObjects()
        {
            return objects_;
        }
        [[nodiscard]] size_t getObjectCount() const
        {
            return objects_.size();
        }
        [[nodiscard]] uint32_t getNextObjectId()
        {
            return nextObjectId_++;
        }

        // 根据对象ID查找对象
        [[nodiscard]] std::shared_ptr<Object3D> getObjectById(uint32_t objectId)
        {
            for (auto &obj : objects_)
            {
                if (obj->getObjectId() == objectId)
                    return obj;
            }
            return nullptr;
        }

        // 清除所有选中状态
        void clearSelection()
        {
            for (auto &obj : objects_)
            {
                obj->setSelected(false);
            }
        }

        // 获取选中的对象
        [[nodiscard]] std::vector<std::shared_ptr<Object3D>> getSelectedObjects() const
        {
            std::vector<std::shared_ptr<Object3D>> selected;
            for (auto &obj : objects_)
            {
                if (obj->isSelected())
                    selected.push_back(obj);
            }
            return selected;
        }

        // 遍历所有可见对象
        void traverse(std::function<void(std::shared_ptr<Object3D>)> callback)
        {
            for (auto &obj : objects_)
            {
                if (obj && obj->isVisible())
                {
                    callback(obj);
                }
            }
        }

      private:
        std::vector<std::shared_ptr<Object3D>> objects_;
        uint32_t nextObjectId_; // 下一个可用的对象ID
    };
} // namespace mcs::vulkan::core

namespace geometry = mcs::vulkan::geometry;

// 网格类：管理顶点和索引缓冲区
class Mesh
{
  public:
    using index_type = uint32_t;

    Mesh(physical_device &physicalDevice, logical_device &device, VkQueue queue,
         VkCommandPool commandPool, std::shared_ptr<geometry::Geometry> geometry)
        : physicalDevice_(physicalDevice), device_(device), queue_(queue),
          commandPool_(commandPool), geometry_(std::move(geometry))
    {
        setupFromGeometry();
    }

    // 从几何体设置网格数据
    void setupFromGeometry()
    {
        if (!geometry_)
            return;
        vertices_ = geometry_->getVertices();
        indices_ = geometry_->getIndices();
        setupAllBuffers();
    }

    // 设置所有帧的缓冲区（双缓冲）
    void setupAllBuffers()
    {
        for (auto &fb : frameBuffers)
        {
            createVertexBufferForFrame(fb);
            createIndexBufferForFrame(fb);
        }
    }

    // 映射索引类型到Vulkan索引类型
    template <typename T>
    static consteval VkIndexType mapIndexType()
    {
        if constexpr (std::same_as<T, uint32_t>)
            return VK_INDEX_TYPE_UINT32;
        else if constexpr (std::same_as<T, uint16_t>)
            return VK_INDEX_TYPE_UINT16;
        else if constexpr (std::same_as<T, uint8_t>)
            return VK_INDEX_TYPE_UINT8;
        else
            throw;
    }

    // 绑定顶点和索引缓冲区
    void bind(VkCommandBuffer commandBuffer, uint32_t currentFrame) const noexcept
    {
        VkBuffer vertex = frameBuffers[currentFrame].vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, offsets);
        vkCmdBindIndexBuffer(commandBuffer,
                             frameBuffers[currentFrame].indexBuffer.buffer(), 0,
                             mapIndexType<index_type>());
    }

    // 绘制网格
    void draw(VkCommandBuffer commandBuffer) const noexcept
    {
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices_.size()), 1, 0, 0,
                         0);
    }

    [[nodiscard]] const std::vector<Vertex> &getVertices() const
    {
        return vertices_;
    }
    [[nodiscard]] const std::vector<index_type> &getIndices() const
    {
        return indices_;
    }
    [[nodiscard]] std::shared_ptr<geometry::Geometry> getGeometry() const
    {
        return geometry_;
    }

  private:
    struct FrameBuffers
    {
        mcs::vulkan::buffer_base vertexBuffer;
        mcs::vulkan::buffer_base indexBuffer;
    };

    physical_device &physicalDevice_;
    logical_device &device_;
    VkQueue queue_;
    VkCommandPool commandPool_;
    std::shared_ptr<geometry::Geometry> geometry_;
    std::vector<Vertex> vertices_;
    std::vector<index_type> indices_;
    std::array<FrameBuffers, MAX_FRAMES_IN_FLIGHT> frameBuffers;

    // 为特定帧创建顶点缓冲区
    void createVertexBufferForFrame(FrameBuffers &fb)
    {
        if (vertices_.empty())
            return;

        const VkDeviceSize BUFFER_SIZE = sizeof(vertices_[0]) * vertices_.size();
        fb.vertexBuffer =
            mcs::vulkan::create_buffer(physicalDevice_, device_,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = BUFFER_SIZE,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::staging_buffer stagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice_, device_,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = BUFFER_SIZE,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        stagingBuffer.mapAndUnmapMemory(vertices_.data(),
                                        static_cast<size_t>(BUFFER_SIZE));
        mcs::vulkan::copy_buffer(device_, queue_, commandPool_, stagingBuffer.buffer(),
                                 fb.vertexBuffer.buffer(), BUFFER_SIZE);
    }

    // 为特定帧创建索引缓冲区
    void createIndexBufferForFrame(FrameBuffers &fb)
    {
        if (indices_.empty())
            return;

        const VkDeviceSize BUFFER_SIZE = sizeof(indices_[0]) * indices_.size();
        fb.indexBuffer = mcs::vulkan::create_buffer(
            physicalDevice_, device_,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = BUFFER_SIZE,
             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::staging_buffer stagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice_, device_,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = BUFFER_SIZE,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        stagingBuffer.mapAndUnmapMemory(indices_.data(),
                                        static_cast<size_t>(BUFFER_SIZE));
        mcs::vulkan::copy_buffer(device_, queue_, commandPool_, stagingBuffer.buffer(),
                                 fb.indexBuffer.buffer(), BUFFER_SIZE);
    }
};

// 统一缓冲区管理器：管理每个对象的Uniform Buffer
struct UniformBufferManager
{
    std::vector<mcs::vulkan::buffer_base> uniformBuffers;
    std::vector<void *> mappedMemory;
    size_t bufferSize;

    UniformBufferManager(physical_device &physicalDevice, logical_device &device,
                         size_t count, size_t size = sizeof(CameraUBO))
        : bufferSize(size)
    {
        uniformBuffers.resize(count);
        mappedMemory.resize(count);

        for (size_t i = 0; i < count; i++)
        {
            uniformBuffers[i] =
                mcs::vulkan::create_buffer(physicalDevice, device,
                                           {.sType = sType<VkBufferCreateInfo>(),
                                            .size = bufferSize,
                                            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            mappedMemory[i] = uniformBuffers[i].map(bufferSize);
        }
    }

    ~UniformBufferManager()
    {
        for (size_t i = 0; i < uniformBuffers.size(); i++)
        {
            if (mappedMemory[i])
            {
                uniformBuffers[i].unmap();
            }
        }
    }

    // 更新指定索引的Uniform Buffer数据
    void updateBuffer(uint32_t index, const CameraUBO &ubo)
    {
        if (index < mappedMemory.size() && mappedMemory[index])
        {
            memcpy(mappedMemory[index], &ubo, sizeof(CameraUBO));
        }
    }
};

// 渲染对象数据：关联对象、网格、Uniform Buffer和描述符集
struct RenderObjectData
{
    std::shared_ptr<mcs::vulkan::core::Object3D> object;
    std::unique_ptr<Mesh> mesh;
    std::unique_ptr<UniformBufferManager> uniformBufferManager;
    std::vector<VkDescriptorSet> descriptorSets;

    RenderObjectData(std::shared_ptr<mcs::vulkan::core::Object3D> obj,
                     std::unique_ptr<Mesh> m,
                     std::unique_ptr<UniformBufferManager> ubManager,
                     std::vector<VkDescriptorSet> descSets)
        : object(std::move(obj)), mesh(std::move(m)),
          uniformBufferManager(std::move(ubManager)), descriptorSets(std::move(descSets))
    {
    }
};

namespace
{
    // 查找支持的深度格式
    VkFormat findDepthFormat(physical_device &physicalDevice)
    {
        std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT,
                                            VK_FORMAT_D32_SFLOAT_S8_UINT,
                                            VK_FORMAT_D24_UNORM_S8_UINT};

        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice.raw_data(), format,
                                                &props);

            if (props.optimalTilingFeatures &
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                return format;
            }
        }
        throw std::runtime_error("failed to find supported depth format!");
    }
} // namespace

// 颜色拾取系统：通过渲染对象ID到颜色缓冲区实现鼠标拾取
struct ColorPickingSystem
{
    VkImage colorPickingImage = VK_NULL_HANDLE; // 颜色拾取图像
    VkDeviceMemory colorPickingImageMemory = VK_NULL_HANDLE;
    VkImageView colorPickingImageView = VK_NULL_HANDLE;
    VkPipelineLayout pickingPipelineLayout = VK_NULL_HANDLE; // 拾取管线布局
    VkPipeline pickingPipeline = VK_NULL_HANDLE;             // 拾取渲染管线
    VkFence pickingFence = VK_NULL_HANDLE;                   // 拾取操作围栏

    VkBuffer pixelBuffer = VK_NULL_HANDLE; // 像素读取缓冲区
    VkDeviceMemory pixelBufferMemory = VK_NULL_HANDLE;

    VkExtent2D extent;                          // 图像尺寸
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM; // 图像格式
    logical_device *device = nullptr;           // 指向逻辑设备的指针

    // 创建颜色拾取系统资源
    void createResources(physical_device &physicalDevice, logical_device &dev,
                         VkExtent2D swapChainExtent,
                         VkDescriptorSetLayout descriptorSetLayout)
    {
        device = &dev;
        extent = swapChainExtent;

        createColorPickingImage(physicalDevice);
        createPickingPipeline(descriptorSetLayout);
        createPixelBuffer(physicalDevice);
        createPickingFence();
    }

    // 清理所有资源
    void cleanup()
    {
        if (device == nullptr)
            return;

        if (pickingFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device->raw_data(), pickingFence, nullptr);
            pickingFence = VK_NULL_HANDLE;
        }

        if (pixelBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device->raw_data(), pixelBuffer, nullptr);
            pixelBuffer = VK_NULL_HANDLE;
        }

        if (pixelBufferMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device->raw_data(), pixelBufferMemory, nullptr);
            pixelBufferMemory = VK_NULL_HANDLE;
        }

        if (colorPickingImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device->raw_data(), colorPickingImageView, nullptr);
            colorPickingImageView = VK_NULL_HANDLE;
        }

        if (colorPickingImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(device->raw_data(), colorPickingImage, nullptr);
            colorPickingImage = VK_NULL_HANDLE;
        }

        if (colorPickingImageMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device->raw_data(), colorPickingImageMemory, nullptr);
            colorPickingImageMemory = VK_NULL_HANDLE;
        }

        if (pickingPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device->raw_data(), pickingPipeline, nullptr);
            pickingPipeline = VK_NULL_HANDLE;
        }

        if (pickingPipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device->raw_data(), pickingPipelineLayout, nullptr);
            pickingPipelineLayout = VK_NULL_HANDLE;
        }

        device = nullptr;
    }

    // 读取鼠标位置像素并返回对象ID
    uint32_t readPixelAtMousePos(VkQueue graphicsQueue, VkCommandPool commandPool,
                                 int mouseX, int mouseY)
    {
        if (!device || mouseX < 0 || mouseY < 0 ||
            mouseX >= static_cast<int>(extent.width) ||
            mouseY >= static_cast<int>(extent.height))
            return 0;

        vkWaitForFences(device->raw_data(), 1, &pickingFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device->raw_data(), 1, &pickingFence);

        VkCommandBufferAllocateInfo allocInfo = {.sType =
                                                     sType<VkCommandBufferAllocateInfo>(),
                                                 .commandPool = commandPool,
                                                 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                 .commandBufferCount = 1};

        VkCommandBuffer copyCmd;
        vkAllocateCommandBuffers(device->raw_data(), &allocInfo, &copyCmd);

        VkCommandBufferBeginInfo beginInfo = {
            .sType = sType<VkCommandBufferBeginInfo>(),
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        vkBeginCommandBuffer(copyCmd, &beginInfo);

        // 转换图像布局以便复制
        render::transition_image_layout(
            copyCmd, colorPickingImage, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .mipLevel = 0,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1},
            .imageOffset = {.x = static_cast<int32_t>(mouseX),
                            .y = static_cast<int32_t>(mouseY),
                            .z = 0},
            .imageExtent = {.width = 1, .height = 1, .depth = 1}};

        vkCmdCopyImageToBuffer(copyCmd, colorPickingImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pixelBuffer, 1,
                               &region);

        // 转换回原来的布局
        render::transition_image_layout(
            copyCmd, colorPickingImage, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        vkEndCommandBuffer(copyCmd);

        VkSubmitInfo submitInfo = {.sType = sType<VkSubmitInfo>(),
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &copyCmd};

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, pickingFence);
        vkWaitForFences(device->raw_data(), 1, &pickingFence, VK_TRUE, UINT64_MAX);

        // 映射内存并读取像素值
        void *data;
        vkMapMemory(device->raw_data(), pixelBufferMemory, 0, 4, 0, &data);
        uint8_t *pixel = static_cast<uint8_t *>(data);
        uint32_t objectId = (pixel[0] << 16) | (pixel[1] << 8) | pixel[2]; // RGB转ID
        vkUnmapMemory(device->raw_data(), pixelBufferMemory);

        vkFreeCommandBuffers(device->raw_data(), commandPool, 1, &copyCmd);
        return objectId;
    }

  private:
    // 创建颜色拾取图像
    void createColorPickingImage(physical_device &physicalDevice)
    {
        VkImageCreateInfo imageInfo = {
            .sType = sType<VkImageCreateInfo>(),
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {.width = extent.width, .height = extent.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        colorPickingImage = device->createImage(imageInfo);

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device->raw_data(), colorPickingImage,
                                     &memRequirements);

        VkMemoryAllocateInfo allocInfo = {
            .sType = sType<VkMemoryAllocateInfo>(),
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = physicalDevice.findMemoryType(
                memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

        colorPickingImageMemory = device->allocateMemory(allocInfo);
        vkBindImageMemory(device->raw_data(), colorPickingImage, colorPickingImageMemory,
                          0);

        VkImageViewCreateInfo viewInfo = {
            .sType = sType<VkImageViewCreateInfo>(),
            .image = colorPickingImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};

        colorPickingImageView = device->createImageView(viewInfo);
    }

    // 创建拾取渲染管线
    void createPickingPipeline(VkDescriptorSetLayout descriptorSetLayout)
    {
        mcs::vulkan::shader_module pickingVertShader{*device, PICKING_VERT_SHADER_PATH};
        mcs::vulkan::shader_module pickingFragShader{*device, PICKING_FRAG_SHADER_PATH};

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
            .sType = sType<VkPipelineShaderStageCreateInfo>(),
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = pickingVertShader.raw_data(),
            .pName = "main"};

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
            .sType = sType<VkPipelineShaderStageCreateInfo>(),
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = pickingFragShader.raw_data(),
            .pName = "main"};

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                          fragShaderStageInfo};
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getPickingAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
            .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount =
                static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data()};

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType = sType<VkPipelineInputAssemblyStateCreateInfo>(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE};

        VkPipelineViewportStateCreateInfo viewportState = {
            .sType = sType<VkPipelineViewportStateCreateInfo>(),
            .viewportCount = 1,
            .scissorCount = 1};

        VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0F};

        VkPipelineMultisampleStateCreateInfo multisampling = {
            .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE};

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        VkPipelineColorBlendStateCreateInfo colorBlending = {
            .sType = sType<VkPipelineColorBlendStateCreateInfo>(),
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment};

        VkPipelineDepthStencilStateCreateInfo depthStencil = {
            .sType = sType<VkPipelineDepthStencilStateCreateInfo>(),
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_ALWAYS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE};

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState = {
            .sType = sType<VkPipelineDynamicStateCreateInfo>(),
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()};

        // Push constant用于传递对象ID到片段着色器
        VkPushConstantRange pushConstantRange = {.stageFlags =
                                                     VK_SHADER_STAGE_FRAGMENT_BIT,
                                                 .offset = 0,
                                                 .size = sizeof(uint32_t)};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = sType<VkPipelineLayoutCreateInfo>(),
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange};

        pickingPipelineLayout = device->createPipelineLayout(pipelineLayoutInfo, nullptr);

        VkFormat colorAttachmentFormat = format;
        VkPipelineRenderingCreateInfo renderingInfo = {
            .sType = sType<VkPipelineRenderingCreateInfo>(),
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorAttachmentFormat};

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = sType<VkGraphicsPipelineCreateInfo>(),
            .pNext = &renderingInfo,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = pickingPipelineLayout,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1};

        pickingPipeline = device->createGraphicsPipelines(VK_NULL_HANDLE, 1,
                                                          pipelineCreateInfo, nullptr);
    }

    // 创建像素读取缓冲区
    void createPixelBuffer(physical_device &physicalDevice)
    {
        VkBufferCreateInfo bufferInfo = {.sType = sType<VkBufferCreateInfo>(),
                                         .size = 4,
                                         .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

        pixelBuffer = device->createBuffer(bufferInfo);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device->raw_data(), pixelBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {
            .sType = sType<VkMemoryAllocateInfo>(),
            .allocationSize = memRequirements.size,
            .memoryTypeIndex =
                physicalDevice.findMemoryType(memRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        pixelBufferMemory = device->allocateMemory(allocInfo);
        vkBindBufferMemory(device->raw_data(), pixelBuffer, pixelBufferMemory, 0);
    }

    // 创建拾取操作围栏
    void createPickingFence()
    {
        VkFenceCreateInfo fenceInfo = {.sType = sType<VkFenceCreateInfo>(), .flags = 0};
        if (vkCreateFence(device->raw_data(), &fenceInfo, nullptr, &pickingFence) !=
            VK_SUCCESS)
            throw std::runtime_error("failed to create picking fence!");
    }
};

// 渲染器类：管理所有渲染对象和渲染流程
class Renderer
{
  public:
    Renderer(physical_device &physicalDevice, logical_device &device,
             VkQueue graphicsQueue, VkCommandPool commandPool,
             VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
             VkPipelineLayout pipelineLayout,
             std::shared_ptr<mcs::vulkan::camera::Camera> camera)
        : physicalDevice_(physicalDevice), device_(device), graphicsQueue_(graphicsQueue),
          commandPool_(commandPool), descriptorPool_(descriptorPool),
          descriptorSetLayout_(descriptorSetLayout), pipelineLayout_(pipelineLayout),
          camera_(std::move(camera))
    {
        createPickingCommandBuffers();
    }

    ~Renderer()
    {
        cleanup();
    }

    // 添加渲染对象
    void addObject(std::shared_ptr<mcs::vulkan::core::Object3D> object)
    {
        if (!object || !object->getGeometry())
            return;

        auto mesh = std::make_unique<Mesh>(physicalDevice_, device_, graphicsQueue_,
                                           commandPool_, object->getGeometry());

        auto uniformBufferManager = std::make_unique<UniformBufferManager>(
            physicalDevice_, device_, MAX_FRAMES_IN_FLIGHT);

        std::vector<VkDescriptorSet> descriptorSets(MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                   descriptorSetLayout_);

        VkDescriptorSetAllocateInfo allocInfo{
            .sType = sType<VkDescriptorSetAllocateInfo>(),
            .descriptorPool = descriptorPool_,
            .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
            .pSetLayouts = layouts.data()};

        if (vkAllocateDescriptorSets(device_.raw_data(), &allocInfo,
                                     descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        // 更新描述符集指向Uniform Buffer
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkDescriptorBufferInfo bufferInfo{
                .buffer = uniformBufferManager->uniformBuffers[i].buffer(),
                .offset = 0,
                .range = sizeof(CameraUBO)};

            VkWriteDescriptorSet descriptorWrite{.sType = sType<VkWriteDescriptorSet>(),
                                                 .dstSet = descriptorSets[i],
                                                 .dstBinding = 0,
                                                 .dstArrayElement = 0,
                                                 .descriptorCount = 1,
                                                 .descriptorType =
                                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                 .pBufferInfo = &bufferInfo};

            vkUpdateDescriptorSets(device_.raw_data(), 1, &descriptorWrite, 0, nullptr);
        }

        auto renderObjectData = std::make_unique<RenderObjectData>(
            object, std::move(mesh), std::move(uniformBufferManager),
            std::move(descriptorSets));

        renderObjects_.push_back(std::move(renderObjectData));
    }

    void addObjects(
        const std::vector<std::shared_ptr<mcs::vulkan::core::Object3D>> &objects)
    {
        for (const auto &obj : objects)
        {
            addObject(obj);
        }
    }

    void removeObject(std::shared_ptr<mcs::vulkan::core::Object3D> object)
    {
        auto it = std::find_if(renderObjects_.begin(), renderObjects_.end(),
                               [&](const auto &rod) { return rod->object == object; });
        if (it != renderObjects_.end())
        {
            renderObjects_.erase(it);
        }
    }

    void clear()
    {
        renderObjects_.clear();
    }

    // 更新所有渲染对象的Uniform Buffer
    void updateUniformBuffers(uint32_t currentFrame)
    {
        for (auto &renderObject : renderObjects_)
        {
            if (!renderObject->object->isVisible())
                continue;

            CameraUBO ubo{};
            ubo.model = renderObject->object->getModelMatrix();
            ubo.view = camera_->get_view_matrix();
            ubo.proj = camera_->get_projection_matrix();

            renderObject->uniformBufferManager->updateBuffer(currentFrame, ubo);
        }
    }

    // 记录主渲染命令到命令缓冲区
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame,
                             uint32_t imageIndex)
    {
        for (auto &renderObject : renderObjects_)
        {
            if (!renderObject->object->isVisible())
                continue;

            vkCmdBindDescriptorSets(
                commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                &renderObject->descriptorSets[currentFrame], 0, nullptr);

            // 传递悬停和选中状态给着色器
            uint32_t isHovered = renderObject->object->isHovered() ? 1u : 0u;
            uint32_t isSelected = renderObject->object->isSelected() ? 1u : 0u;
            uint32_t pushConstants[2] = {isHovered, isSelected};
            vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT,
                               0, 2 * sizeof(uint32_t), pushConstants);

            renderObject->mesh->bind(commandBuffer, currentFrame);
            renderObject->mesh->draw(commandBuffer);
        }
    }

    void setColorPickingSystem(ColorPickingSystem *colorPickingSystem)
    {
        colorPickingSystem_ = colorPickingSystem;
    }

    // 使用颜色拾取系统更新鼠标悬停状态
    void updateHoverWithColorPicking(int mouseX, int mouseY, uint32_t currentFrame,
                                     std::shared_ptr<mcs::vulkan::core::Scene> scene)
    {
        if (!colorPickingSystem_)
            return;

        for (auto &renderObject : renderObjects_)
        {
            renderObject->object->setHovered(false);
        }

        performPickingRender(currentFrame);

        uint32_t objectId = colorPickingSystem_->readPixelAtMousePos(
            graphicsQueue_, commandPool_, mouseX, mouseY);

        if (objectId > 0)
        {
            auto object = scene->getObjectById(objectId);
            if (object)
            {
                object->setHovered(true);
            }
        }
    }

    // 处理鼠标点击事件
    bool handleMouseClick(int mouseX, int mouseY, uint32_t currentFrame,
                          std::shared_ptr<mcs::vulkan::core::Scene> scene,
                          bool ctrlPressed = false)
    {
        if (!colorPickingSystem_)
            return false;

        performPickingRender(currentFrame);

        uint32_t objectId = colorPickingSystem_->readPixelAtMousePos(
            graphicsQueue_, commandPool_, mouseX, mouseY);

        if (objectId > 0)
        {
            auto object = scene->getObjectById(objectId);
            if (object)
            {
                if (ctrlPressed)
                {
                    // Ctrl+点击：切换选中状态
                    object->setSelected(!object->isSelected());
                }
                else
                {
                    // 普通点击：清除其他选中，只选中当前对象
                    scene->clearSelection();
                    object->setSelected(true);
                }
                return true;
            }
        }
        else
        {
            // 点击空白处：清除所有选中
            scene->clearSelection();
        }
        return false;
    }

    // 执行拾取渲染（将对象ID渲染到颜色缓冲区）
    void performPickingRender(uint32_t currentFrame)
    {
        if (!colorPickingSystem_)
            return;

        VkCommandBuffer commandBuffer = pickingCommandBuffers_[currentFrame];

        VkCommandBufferBeginInfo beginInfo = {
            .sType = sType<VkCommandBufferBeginInfo>(),
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        render::transition_image_layout(
            commandBuffer, colorPickingSystem_->colorPickingImage,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkRenderingAttachmentInfo colorAttachment = {
            .sType = sType<VkRenderingAttachmentInfo>(),
            .imageView = colorPickingSystem_->colorPickingImageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}};

        VkRenderingInfo renderingInfo = {
            .sType = sType<VkRenderingInfo>(),
            .renderArea = {.offset = {.x = 0, .y = 0},
                           .extent = colorPickingSystem_->extent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment};

        vkCmdBeginRendering(commandBuffer, &renderingInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          colorPickingSystem_->pickingPipeline);

        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(colorPickingSystem_->extent.width),
            .height = static_cast<float>(colorPickingSystem_->extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {.offset = {.x = 0, .y = 0},
                            .extent = colorPickingSystem_->extent};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        for (auto &renderObject : renderObjects_)
        {
            if (!renderObject->object->isVisible())
                continue;

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    colorPickingSystem_->pickingPipelineLayout, 0, 1,
                                    &renderObject->descriptorSets[currentFrame], 0,
                                    nullptr);

            uint32_t objectId = renderObject->object->getObjectId();
            vkCmdPushConstants(commandBuffer, colorPickingSystem_->pickingPipelineLayout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t),
                               &objectId);

            renderObject->mesh->bind(commandBuffer, currentFrame);
            renderObject->mesh->draw(commandBuffer);
        }

        vkCmdEndRendering(commandBuffer);

        render::transition_image_layout(
            commandBuffer, colorPickingSystem_->colorPickingImage,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {.sType = sType<VkSubmitInfo>(),
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &commandBuffer};

        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);
    }

    [[nodiscard]] size_t getRenderObjectCount() const
    {
        return renderObjects_.size();
    }

    void cleanup()
    {
        if (!pickingCommandBuffers_.empty())
        {
            vkFreeCommandBuffers(device_.raw_data(), commandPool_,
                                 static_cast<uint32_t>(pickingCommandBuffers_.size()),
                                 pickingCommandBuffers_.data());
            pickingCommandBuffers_.clear();
        }
    }

  private:
    // 创建拾取命令缓冲区
    void createPickingCommandBuffers()
    {
        VkCommandBufferAllocateInfo allocInfo = {
            .sType = sType<VkCommandBufferAllocateInfo>(),
            .commandPool = commandPool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

        pickingCommandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateCommandBuffers(device_.raw_data(), &allocInfo,
                                     pickingCommandBuffers_.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate picking command buffers!");
        }
    }

    physical_device &physicalDevice_;
    logical_device &device_;
    VkQueue graphicsQueue_;
    VkCommandPool commandPool_;
    VkDescriptorPool descriptorPool_;
    VkDescriptorSetLayout descriptorSetLayout_;
    VkPipelineLayout pipelineLayout_;
    std::shared_ptr<mcs::vulkan::camera::Camera> camera_;
    std::vector<std::unique_ptr<RenderObjectData>> renderObjects_;
    ColorPickingSystem *colorPickingSystem_ = nullptr;
    std::vector<VkCommandBuffer> pickingCommandBuffers_;
};

int main()
{
    try
    {
        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features>
            enablefeatureChain = {
                {}, {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE}};

        // 创建窗口和输入
        surface window{};
        window.setup({.width = WIDTH, .height = HEIGHT}, TITLE);
        glfw_input input;

        // 创建相机和控制器
        using namespace mcs::vulkan::camera;
        auto camera = std::make_shared<Camera>();
        auto camera_controller = std::make_shared<CameraController>(camera);

        camera->set_position(glm::vec3(0.0f, 0.0f, 5.0f));
        camera->set_perspective(45.0f, static_cast<float>(WIDTH) / HEIGHT, 0.1f, 100.0f);

        // 创建场景
        using namespace mcs::vulkan::core;
        using namespace mcs::vulkan::geometry;

        auto scene = std::make_shared<Scene>();
        auto cubeGeometry = std::make_shared<CubeGeometry>(1.0f);
        auto planeGeometry = std::make_shared<PlaneGeometry>(3.0f, 3.0f);
        auto triangleGeometry = std::make_shared<TriangleGeometry>();

        // 创建场景对象
        auto cube1 = std::make_shared<Object3D>(cubeGeometry, scene->getNextObjectId());
        cube1->setPosition(glm::vec3(-1.5f, 0.0f, 0.0f));
        cube1->setName("Red Cube");

        auto cube2 = std::make_shared<Object3D>(cubeGeometry, scene->getNextObjectId());
        cube2->setPosition(glm::vec3(1.5f, 0.0f, 0.0f));
        cube2->setRotation(
            glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        cube2->setName("Rotated Cube");

        auto plane = std::make_shared<Object3D>(planeGeometry, scene->getNextObjectId());
        plane->setPosition(glm::vec3(0.0f, -1.0f, 0.0f));
        plane->setName("Ground Plane");

        auto triangle =
            std::make_shared<Object3D>(triangleGeometry, scene->getNextObjectId());
        triangle->setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
        triangle->setScale(0.5f);
        triangle->setName("Triangle");

        auto triangle2 =
            std::make_shared<Object3D>(triangleGeometry, scene->getNextObjectId());
        triangle2->setPosition(glm::vec3(0.1f, 1.0f, 0.5f));
        triangle2->setScale(0.5f);
        triangle2->setName("Triangle");

        scene->add({cube1, cube2, plane, triangle, triangle2});

        // 创建Vulkan实例
        auto instance =
            make_instance{}
                .enableDebugExtension()
                .enableSurfaceExtension<surface>()
                .checkExtensionSupport()
                .checkLayerSupport()
                .build({.sType = sType<VkApplicationInfo>(),
                        .pApplicationName = "Scene Test with Color Picking and Click",
                        .applicationVersion = VkApiVersion(1, 0, 0),
                        .pEngineName = "No Engine",
                        .engineVersion = VkApiVersion(1, 0, 0),
                        .apiVersion = VkApiVersion(0, 1, 3, 0)});

        VkSurfaceKHR surface_ = window.createVkSurfaceKHR(instance.ref_data());
        assert(surface_ != nullptr);

        // 选择物理设备
        auto physicalDevice =
            make_physical_device{instance.ref_data()}
                .requiredDeviceProperties([](const VkPhysicalDeviceProperties
                                                 &device_properties) constexpr noexcept {
                    return device_properties.apiVersion >= VK_API_VERSION_1_3;
                })
                .requiredQueueFamilyProperties(
                    [](const VkQueueFamilyProperties &qfp) constexpr noexcept {
                        return !!(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT);
                    })
                .requiredDeviceExtensions(requiredDeviceExtension)
                .requiredFeatures(
                    [](const physical_device &physicalDevice) constexpr noexcept -> bool {
                        auto query =
                            structure_chain<VkPhysicalDeviceFeatures2,
                                            VkPhysicalDeviceVulkan13Features>{{}, {}};
                        physicalDevice.getFeatures2(query.head());
                        auto &query_vulkan13_features =
                            query.template get<VkPhysicalDeviceVulkan13Features>();
                        return query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2;
                    })
                .pickPhysicalDevice();

        auto graphicsIndex =
            make_queue_family_index{physicalDevice}
                .requiredQueueFamilyProperties(
                    [&](const VkQueueFamilyProperties &qfp, uint32_t queueFamilyIndex,
                        const physical_device &physicalDevice) noexcept -> bool {
                        return (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                               physicalDevice.getSurfaceSupportKHR(queueFamilyIndex,
                                                                   surface_);
                    })
                .build();

        // 创建设备
        auto logical_device_ = [&]() {
            float queuePriority = 0.0F;
            VkDeviceQueueCreateInfo deviceQueueCreateInfo{
                .sType = sType<VkDeviceQueueCreateInfo>(),
                .queueFamilyIndex = graphicsIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority};
            VkDeviceCreateInfo deviceCreateInfo{
                .sType = sType<VkDeviceCreateInfo>(),
                .pNext = &enablefeatureChain.head(),
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &deviceQueueCreateInfo,
                .enabledExtensionCount =
                    static_cast<uint32_t>(requiredDeviceExtension.size()),
                .ppEnabledExtensionNames = requiredDeviceExtension.data(),
            };
            return logical_device{
                physicalDevice.createDevice(&deviceCreateInfo, nullptr)};
        }();

        auto *graphicsQueue = logical_device_.getDeviceQueue(graphicsIndex, 0);

        // 查找呈现队列族
        auto presentIndex = [&]() {
            std::vector<VkQueueFamilyProperties> queueFamilyProperties =
                physicalDevice.getQueueFamilyProperties();
            auto presentIndex =
                physicalDevice.getSurfaceSupportKHR(graphicsIndex, surface_)
                    ? graphicsIndex
                    : ~0;

            if (presentIndex == ~0)
            {
                for (size_t i = 0; i < queueFamilyProperties.size(); i++)
                {
                    if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i),
                                                            surface_))
                    {
                        presentIndex = static_cast<uint32_t>(i);
                        break;
                    }
                }
            }

            if (presentIndex == ~0)
                throw std::runtime_error(
                    "Could not find a queue for present -> terminating");
            return presentIndex;
        }();

        auto *presentQueue = logical_device_.getDeviceQueue(presentIndex, 0);

        // 交换链相关变量
        VkFormat swapChainImageFormat{};
        VkExtent2D swapChainExtent;
        VkSwapchainKHR swapChain{};
        std::vector<VkImage> swapChainImages{};
        std::vector<VkImageView> swapChainImageViews;

        // 深度资源
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkImage depthImage = VK_NULL_HANDLE;
        VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;

        // 图形管线资源
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

        // 描述符资源
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        // 命令池
        VkCommandPool commandPool = VK_NULL_HANDLE;

        // 颜色拾取系统
        ColorPickingSystem colorPickingSystem;

        // 创建深度资源
        auto createDepthResources = [&]() {
            depthFormat = findDepthFormat(physicalDevice);
            VkImageCreateInfo imageInfo = {
                .sType = sType<VkImageCreateInfo>(),
                .imageType = VK_IMAGE_TYPE_2D,
                .format = depthFormat,
                .extent = {.width = swapChainExtent.width,
                           .height = swapChainExtent.height,
                           .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

            depthImage = logical_device_.createImage(imageInfo);

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(logical_device_.raw_data(), depthImage,
                                         &memRequirements);

            VkMemoryAllocateInfo allocInfo = {
                .sType = sType<VkMemoryAllocateInfo>(),
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = physicalDevice.findMemoryType(
                    memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

            depthImageMemory = logical_device_.allocateMemory(allocInfo);
            vkBindImageMemory(logical_device_.raw_data(), depthImage, depthImageMemory,
                              0);

            VkImageViewCreateInfo viewInfo = {
                .sType = sType<VkImageViewCreateInfo>(),
                .image = depthImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = depthFormat,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};

            depthImageView = logical_device_.createImageView(viewInfo);
        };

        // 清理深度资源
        auto cleanupDepthResources = [&]() {
            if (depthImageView != VK_NULL_HANDLE)
            {
                logical_device_.destroyImageView(depthImageView);
                depthImageView = VK_NULL_HANDLE;
            }
            if (depthImage != VK_NULL_HANDLE)
            {
                logical_device_.destroyImage(depthImage);
                depthImage = VK_NULL_HANDLE;
            }
            if (depthImageMemory != VK_NULL_HANDLE)
            {
                logical_device_.freeMemory(depthImageMemory);
                depthImageMemory = VK_NULL_HANDLE;
            }
        };

        // 创建描述符布局和池
        {
            VkDescriptorSetLayoutBinding uboLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = nullptr};

            VkDescriptorSetLayoutCreateInfo layoutInfo{
                .sType = sType<VkDescriptorSetLayoutCreateInfo>(),
                .bindingCount = 1,
                .pBindings = &uboLayoutBinding};

            descriptorSetLayout = logical_device_.createDescriptorSetLayout(layoutInfo);
        }

        {
            const uint32_t MAX_RENDER_OBJECTS = 10;
            VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          .descriptorCount = static_cast<uint32_t>(
                                              MAX_FRAMES_IN_FLIGHT * MAX_RENDER_OBJECTS)};

            VkDescriptorPoolCreateInfo poolInfo{
                .sType = sType<VkDescriptorPoolCreateInfo>(),
                .maxSets =
                    static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * MAX_RENDER_OBJECTS),
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize};

            descriptorPool = logical_device_.createDescriptorPool(poolInfo);
        }

        // 创建命令池
        {
            VkCommandPoolCreateInfo poolInfo{
                .sType = sType<VkCommandPoolCreateInfo>(),
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = graphicsIndex};
            commandPool = logical_device_.createCommandPool(poolInfo);
        }

        // 创建交换链
        auto createSwapChain = [&]() {
            auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface_);
            auto swapChainFormat = choose_swap_surface_format(
                VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_SRGB,
                                   .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                physicalDevice.getSurfaceFormatsKHR(surface_));
            swapChainImageFormat = swapChainFormat.format;

            swapChainExtent = window.chooseSwapExtent(surfaceCapabilities);

            auto minImageCount = std::max(3U, surfaceCapabilities.minImageCount);
            if (surfaceCapabilities.maxImageCount > 0 &&
                minImageCount > surfaceCapabilities.maxImageCount)
                minImageCount = surfaceCapabilities.maxImageCount;

            VkSwapchainCreateInfoKHR swapChainCreateInfo{
                .sType = sType<VkSwapchainCreateInfoKHR>(),
                .surface = surface_,
                .minImageCount = minImageCount,
                .imageFormat = swapChainImageFormat,
                .imageColorSpace = swapChainFormat.colorSpace,
                .imageExtent = swapChainExtent,
                .imageArrayLayers = 1,
                .imageUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .preTransform = surfaceCapabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = choose_swap_present_mode(
                    VK_PRESENT_MODE_MAILBOX_KHR,
                    physicalDevice.getSurfacePresentModesKHR(surface_)),
                .clipped = true};

            swapChain = logical_device_.createSwapchainKHR(swapChainCreateInfo);
            swapChainImages = logical_device_.getSwapchainImagesKHR(swapChain);
        };

        // 创建图像视图
        auto createImageViews = [&]() {
            swapChainImageViews.clear();
            VkImageViewCreateInfo imageViewCreateInfo{
                .sType = sType<VkImageViewCreateInfo>(),
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapChainImageFormat,
                .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            for (auto *image : swapChainImages)
            {
                imageViewCreateInfo.image = image;
                VkImageView imageView =
                    logical_device_.createImageView(imageViewCreateInfo, nullptr);
                swapChainImageViews.push_back(imageView);
            }
        };

        // 创建图形管线
        auto createGraphicsPipeline = [&]() -> std::pair<VkPipelineLayout, VkPipeline> {
            mcs::vulkan::shader_module vertshader{logical_device_, VERT_SHADER_PATH};
            mcs::vulkan::shader_module fragshader{logical_device_, FRAG_SHADER_PATH};

            VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
                .sType = sType<VkPipelineShaderStageCreateInfo>(),
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertshader.raw_data(),
                .pName = "main"};
            VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
                .sType = sType<VkPipelineShaderStageCreateInfo>(),
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragshader.raw_data(),
                .pName = "main"};

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                              fragShaderStageInfo};

            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &bindingDescription,
                .vertexAttributeDescriptionCount =
                    static_cast<uint32_t>(attributeDescriptions.size()),
                .pVertexAttributeDescriptions = attributeDescriptions.data()};

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                .sType = sType<VkPipelineInputAssemblyStateCreateInfo>(),
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE};

            VkPipelineViewportStateCreateInfo viewportState = {
                .sType = sType<VkPipelineViewportStateCreateInfo>(),
                .viewportCount = 1,
                .scissorCount = 1};

            VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE};

            VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
            VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType = sType<VkPipelineColorBlendStateCreateInfo>(),
                .logicOpEnable = VK_FALSE,
                .logicOp = VkLogicOp::VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment};

            VkPipelineDepthStencilStateCreateInfo depthStencil = {
                .sType = sType<VkPipelineDepthStencilStateCreateInfo>(),
                .depthTestEnable = VK_TRUE,
                .depthWriteEnable = VK_TRUE,
                .depthCompareOp = VK_COMPARE_OP_LESS,
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable = VK_FALSE,
                .front = {},
                .back = {},
                .minDepthBounds = 0.0f,
                .maxDepthBounds = 1.0f};

            std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                         VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            VkPushConstantRange pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = 2 * sizeof(uint32_t)}; // diff: 改为2个uint32_t

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(),
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushConstantRange};

            VkPipelineLayout layout =
                logical_device_.createPipelineLayout(pipelineLayoutInfo, nullptr);
            pipelineLayout = layout;

            structure_chain<VkGraphicsPipelineCreateInfo, VkPipelineRenderingCreateInfo>
                pipelineCreateInfoChain = {
                    {.stageCount = 2,
                     .pStages = shaderStages,
                     .pVertexInputState = &vertexInputInfo,
                     .pInputAssemblyState = &inputAssembly,
                     .pViewportState = &viewportState,
                     .pRasterizationState = &rasterizer,
                     .pMultisampleState = &multisampling,
                     .pDepthStencilState = &depthStencil,
                     .pColorBlendState = &colorBlending,
                     .pDynamicState = &dynamicState,
                     .layout = layout,
                     .renderPass = VK_NULL_HANDLE},
                    {.colorAttachmentCount = 1,
                     .pColorAttachmentFormats = &swapChainImageFormat,
                     .depthAttachmentFormat = depthFormat}};

            VkPipeline pipeline = logical_device_.createGraphicsPipelines(
                nullptr, 1, pipelineCreateInfoChain.head(), nullptr);
            graphicsPipeline = pipeline;

            return {layout, pipeline};
        };

        // 初始化交换链和深度资源
        createSwapChain();
        createImageViews();
        createDepthResources();
        std::tie(pipelineLayout, graphicsPipeline) = createGraphicsPipeline();

        // 创建颜色拾取系统资源
        colorPickingSystem.createResources(physicalDevice, logical_device_,
                                           swapChainExtent, descriptorSetLayout);

        // 创建命令缓冲区
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
        logical_device_.allocateCommandBuffers(
            commandBuffers[0],
            {.sType = sType<VkCommandBufferAllocateInfo>(),
             .commandPool = commandPool,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())});

        FrameContext frameContext{logical_device_, swapChainImages.size()};

        // 创建Renderer
        Renderer renderer(physicalDevice, logical_device_, graphicsQueue, commandPool,
                          descriptorPool, descriptorSetLayout, pipelineLayout, camera);
        renderer.addObjects(scene->getObjects());
        renderer.setColorPickingSystem(&colorPickingSystem);

        // 清理交换链
        auto cleanupSwapChain = [&]() {
            for (auto imageView : swapChainImageViews)
            {
                logical_device_.destroyImageView(imageView, nullptr);
            }
            swapChainImageViews.clear();

            if (swapChain != VK_NULL_HANDLE)
            {
                logical_device_.destroySwapchainKHR(swapChain);
                swapChain = VK_NULL_HANDLE;
            }
        };

        // 重新创建交换链（窗口大小改变时调用）
        auto recreateSwapChain = [&]() {
            window.waitGoodFramebufferSize();
            logical_device_.waitIdle();

            // 清理旧资源
            cleanupSwapChain();
            cleanupDepthResources();
            colorPickingSystem.cleanup();

            // 重新创建资源
            createSwapChain();
            createImageViews();
            createDepthResources();

            // 更新相机宽高比
            auto [w, h] = window.getFramebufferSize();
            camera->set_window_size({w, h});

            // 重新创建颜色拾取系统资源
            colorPickingSystem.createResources(physicalDevice, logical_device_,
                                               swapChainExtent, descriptorSetLayout);
        };

        // 记录命令缓冲区
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];

            render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            render::transition_image_layout(
                commandBuffer, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.1F, 0.1F, 0.1F, 1.0F}}};
            VkClearValue depthClear = {.depthStencil = {.depth = 1.0f}};

            VkRenderingAttachmentInfo colorAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = imageView,
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = clearColor};

            VkRenderingAttachmentInfo depthAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = depthImageView,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = depthClear};

            VkRenderingInfo renderingInfo = {
                .sType = sType<VkRenderingInfo>(),
                .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment,
                .pDepthAttachment = &depthAttachment};

            vkCmdBeginRendering(commandBuffer, &renderingInfo);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              graphicsPipeline);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            renderer.recordCommandBuffer(commandBuffer, currentFrame, imageIndex);

            vkCmdEndRendering(commandBuffer);

            render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        };

        // 时间统计变量
        auto last_time = std::chrono::high_resolution_clock::now();
        float total_time = 0.0f;
        int frame_counter = 0;

        glm::vec2 lastMousePos(0.0f);
        bool mouseMoved = false;
        bool leftMouseButtonPressed = false; // 跟踪鼠标左键按下状态
        bool ctrlPressed = false;            // 跟踪Ctrl键状态

        // 绘制一帧的主要逻辑
        auto drawFrame = [&]() {
            auto &inFlightFences = frameContext.inFlightFences;
            auto &currentFrame = frameContext.currentFrame;
            auto &presentCompleteSemaphore = frameContext.presentCompleteSemaphore;
            auto &semaphoreIndex = frameContext.semaphoreIndex;
            auto &renderFinishedSemaphore = frameContext.renderFinishedSemaphore;

            auto *device = logical_device_.raw_data();

            while (vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                                   UINT64_MAX) == VK_TIMEOUT)
                ;

            auto current_time = std::chrono::high_resolution_clock::now();
            float delta_time =
                std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;

            total_time += delta_time;
            frame_counter++;

            if (total_time >= 1.0f)
            {
                float fps = frame_counter / total_time;
                auto pos = camera->get_position();
                auto selectedObjects = scene->getSelectedObjects();
                std::println(
                    "\n[Frame] FPS: {:.1f}, Delta: {:.3f}ms, Objects: {}, Selected: {}",
                    fps, delta_time * 1000, renderer.getRenderObjectCount(),
                    selectedObjects.size());
                std::println("[Camera] Pos({:.2f}, {:.2f}, {:.2f})", pos.x, pos.y, pos.z);
                if (!selectedObjects.empty())
                {
                    std::println("[Selected] {}", selectedObjects[0]->getName());
                }

                total_time = 0.0f;
                frame_counter = 0;
            }

            input.reset();
            surface::pollEvents();
            camera_controller->update(input, delta_time);

            // 更新场景动画
            scene->update(delta_time);

            // 检查Ctrl键状态
            using namespace mcs::vulkan::event;
            const auto is_key_pressed = [&](Key key) {
                const auto &event = input.get_keyboard_event(key);
                return event.press() || event.repeat();
            };
            ctrlPressed =
                is_key_pressed(Key::LEFT_CONTROL) || is_key_pressed(Key::RIGHT_CONTROL);

            // 检查鼠标移动
            auto cursor_pos = input.cursorPos();
            if (cursor_pos != mcs::vulkan::event::position2d_event{})
            {
                glm::vec2 mousePos(cursor_pos.xpos, cursor_pos.ypos);
                if (glm::length(mousePos - lastMousePos) > 0.5f)
                {
                    mouseMoved = true;
                    lastMousePos = mousePos;
                }
            }

            // 如果鼠标移动了，执行颜色拾取更新悬停状态
            if (mouseMoved)
            {
                renderer.updateHoverWithColorPicking(static_cast<int>(lastMousePos.x),
                                                     static_cast<int>(lastMousePos.y),
                                                     currentFrame, scene);
                mouseMoved = false;
            }

            // 检查鼠标左键点击事件
            const auto &mouse_left =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);
            if (mouse_left.press() && !leftMouseButtonPressed)
            {
                // 鼠标左键刚按下
                leftMouseButtonPressed = true;
            }
            else if (mouse_left.release() && leftMouseButtonPressed)
            {
                // 鼠标左键释放，触发点击事件
                leftMouseButtonPressed = false;

                // 处理点击事件
                bool objectClicked = renderer.handleMouseClick(
                    static_cast<int>(lastMousePos.x), static_cast<int>(lastMousePos.y),
                    currentFrame, scene, ctrlPressed);

                if (objectClicked)
                {
                    std::println("[Click] Object selected at ({}, {})",
                                 static_cast<int>(lastMousePos.x),
                                 static_cast<int>(lastMousePos.y));
                }
            }

            renderer.updateUniformBuffers(currentFrame);

            // 获取交换链图像
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(
                device, swapChain, UINT64_MAX, presentCompleteSemaphore[semaphoreIndex],
                VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreateSwapChain();
                return;
            }
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            vkResetFences(device, 1, &inFlightFences[currentFrame]);
            auto *commandBuffer = commandBuffers[currentFrame];
            vkResetCommandBuffer(commandBuffer, 0);

            recordCommandBuffer(commandBuffer, currentFrame, imageIndex);

            VkPipelineStageFlags waitDestinationStageMask[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

            VkSubmitInfo submitInfo = {
                .sType = sType<VkSubmitInfo>(),
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &presentCompleteSemaphore[semaphoreIndex],
                .pWaitDstStageMask = waitDestinationStageMask,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinishedSemaphore[imageIndex]};

            if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                              inFlightFences[currentFrame]) != VK_SUCCESS)
                throw std::runtime_error("failed to submit draw command buffer!");

            VkPresentInfoKHR presentInfo = {.sType = sType<VkPresentInfoKHR>(),
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores =
                                                &renderFinishedSemaphore[imageIndex],
                                            .swapchainCount = 1,
                                            .pSwapchains = &swapChain,
                                            .pImageIndices = &imageIndex};

            result = vkQueuePresentKHR(presentQueue, &presentInfo);

            if (auto &framebufferResized = window.refFramebufferResized();
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                framebufferResized)
            {
                framebufferResized = false;
                recreateSwapChain();
            }

            semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        };

        std::println("\n=== Scene Test with Color Picking and Click Started ===");
        std::println("Scene contains: {} objects", scene->getObjectCount());
        std::println("Controls:");
        std::println("  - Click: Select object");
        std::println("  - Ctrl+Click: Toggle multiple selection");
        std::println("  - Click empty space: Clear selection");
        std::println("  - Hover: White highlight");
        std::println("  - Selected: Green color with pulse animation");

        // 主渲染循环
        while (window.shouldClose() == 0)
        {
            drawFrame();
        }

        vkDeviceWaitIdle(logical_device_.raw_data());

        // 清理资源（按照正确的顺序）
        // 1. 首先销毁Renderer（释放pickingCommandBuffers_）
        renderer.cleanup();

        // 2. 清理颜色拾取系统
        colorPickingSystem.cleanup();

        // 3. 释放主命令缓冲区
        if (!commandBuffers.empty())
        {
            vkFreeCommandBuffers(logical_device_.raw_data(), commandPool,
                                 static_cast<uint32_t>(commandBuffers.size()),
                                 commandBuffers.data());
        }

        // 4. 销毁其他Vulkan资源
        if (graphicsPipeline != VK_NULL_HANDLE)
        {
            logical_device_.destroyPipeline(graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }

        if (pipelineLayout != VK_NULL_HANDLE)
        {
            logical_device_.destroyPipelineLayout(pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }

        // 5. 清理FrameContext（这会释放同步对象）
        // 注意：FrameContext会在离开作用域时自动析构

        // 6. 清理其他资源
        if (descriptorPool != VK_NULL_HANDLE)
        {
            logical_device_.destroyDescriptorPool(descriptorPool);
            descriptorPool = VK_NULL_HANDLE;
        }

        if (descriptorSetLayout != VK_NULL_HANDLE)
        {
            logical_device_.destroyDescriptorSetLayout(descriptorSetLayout);
            descriptorSetLayout = VK_NULL_HANDLE;
        }

        cleanupSwapChain();
        cleanupDepthResources();

        // 7. 最后销毁命令池
        if (commandPool != VK_NULL_HANDLE)
        {
            logical_device_.destroyCommandPool(commandPool);
            commandPool = VK_NULL_HANDLE;
        }
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface_);
        std::println("\n=== Scene Test with Color Picking and Click Finished ===");
    }
    catch (std::exception &e)
    {
        std::println("Error: {}", e.what());
    }

    return 0;
}