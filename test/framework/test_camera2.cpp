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

// diff: 改进的相机系统
namespace mcs::vulkan::camera
{
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

    class Camera
    {
      public:
        Camera()
        {
            reset();
        }

        void reset()
        {
            position_ = glm::vec3(0.0f, 0.0f, 5.0f);
            yaw_ = 0.0f;
            pitch_ = 0.0f;
            roll_ = 0.0f;
            orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            update_orientation_from_euler();

            set_perspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

            std::println("[Camera] Reset: Position({:.2f}, {:.2f}, {:.2f}), Yaw={:.1f}, "
                         "Pitch={:.1f}",
                         position_.x, position_.y, position_.z, yaw_, pitch_);
        }

        void set_perspective(float fov, float aspect, float near, float far)
        {
            fov_ = fov;
            aspect_ratio_ = aspect;
            near_ = near;
            far_ = far;

            projection_matrix_ =
                glm::perspective(glm::radians(fov_), aspect_ratio_, near_, far_);
        }

        void set_position(const glm::vec3 &position)
        {
            position_ = position;
            update_view_matrix();
        }

        void move(const glm::vec3 &offset)
        {
            position_ += orientation_ * offset;
            update_view_matrix();
        }

        // diff: 新增平移功能（沿着屏幕平面移动）
        void pan(float right_offset, float up_offset)
        {
            position_ += get_right_vector() * right_offset + get_up_vector() * up_offset;
            update_view_matrix();
        }

        void rotate(float yaw, float pitch)
        {
            yaw_ += yaw;
            pitch_ += pitch;

            // 限制俯仰角
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

        void set_window_size(const WindowSize &size)
        {
            if (size.is_valid())
            {
                set_perspective(fov_, size.aspect_ratio(), near_, far_);
            }
        }

      private:
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

        void update_view_matrix()
        {
            view_matrix_ =
                glm::lookAt(position_, position_ + get_forward_vector(), get_up_vector());
        }

        glm::vec3 position_;
        glm::quat orientation_;
        float yaw_ = 0.0f;
        float pitch_ = 0.0f;
        float roll_ = 0.0f;

        glm::mat4 view_matrix_;
        glm::mat4 projection_matrix_;

        float fov_;
        float aspect_ratio_;
        float near_;
        float far_;
    };

    class CameraController
    {
      public:
        explicit CameraController(std::shared_ptr<Camera> camera)
            : camera_(std::move(camera))
        {
            std::println("[CameraController] Created");
        }

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
            std::println("[CameraController] {}", enabled ? "Enabled" : "Disabled");
        }

        // diff: 获取和设置模型旋转
        [[nodiscard]] glm::quat get_model_rotation() const noexcept
        {
            return model_rotation_;
        }

        void reset_model_rotation()
        {
            model_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            std::println("[CameraController] Model rotation reset");
        }

      private:
        void handle_keyboard(glfw_input &input, float delta_time)
        {
            using namespace mcs::vulkan::event;

            const auto is_key_pressed = [&](Key key) {
                const auto &event = input.get_keyboard_event(key);
                return event.press() || event.repeat();
            };

            float speed = 5.0f * delta_time; // 基础速度
            glm::vec3 movement(0.0f);
            bool moved = false;

            // WASDQE 移动
            if (is_key_pressed(Key::W))
            {
                movement.z -= 1.0f;
                moved = true;
                std::println("[CameraController] W pressed - move forward");
            }
            if (is_key_pressed(Key::S))
            {
                movement.z += 1.0f;
                moved = true;
                std::println("[CameraController] S pressed - move backward");
            }
            if (is_key_pressed(Key::A))
            {
                movement.x += 1.0f;
                moved = true;
                std::println("[CameraController] A pressed - move left");
            }
            if (is_key_pressed(Key::D))
            {
                movement.x -= 1.0f;
                moved = true;
                std::println("[CameraController] D pressed - move right");
            }
            if (is_key_pressed(Key::Q))
            {
                movement.y += 1.0f;
                moved = true;
                std::println("[CameraController] Q pressed - move down");
            }
            if (is_key_pressed(Key::E))
            {
                movement.y -= 1.0f;
                moved = true;
                std::println("[CameraController] E pressed - move up");
            }

            // Shift键仍然被检测
            if (is_key_pressed(Key::LEFT_SHIFT) || is_key_pressed(Key::RIGHT_SHIFT))
            {
                std::println("[CameraController] Shift pressed");
            }

            // Ctrl减速
            if (is_key_pressed(Key::LEFT_CONTROL) || is_key_pressed(Key::RIGHT_CONTROL))
            {
                speed *= 0.5f;
                std::println("[CameraController] Ctrl pressed - slow mode");
            }

            if (moved)
            {
                movement = glm::normalize(movement);
                camera_->move(movement * speed);
            }

            // 重置相机
            if (is_key_pressed(Key::R))
            {
                camera_->reset();
                std::println("[CameraController] Camera reset");
            }

            // diff: 重置模型旋转
            if (is_key_pressed(Key::T))
            {
                reset_model_rotation();
            }
        }

        void handle_mouse(glfw_input &input)
        {
            using namespace mcs::vulkan::event;

            // 获取鼠标按键状态
            const auto &mouse_left =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_LEFT);

            // 获取鼠标位置
            auto cursor_pos = input.cursorPos();
            if (cursor_pos == event::position2d_event{})
                return;

            glm::vec2 current_pos(cursor_pos.xpos, cursor_pos.ypos);

            // 计算鼠标移动增量
            glm::vec2 delta = current_pos - last_mouse_pos_;
            last_mouse_pos_ = current_pos;

            // 检查Ctrl键状态，用于切换旋转轴
            using namespace mcs::vulkan::event;
            const auto is_key_pressed = [&](Key key) {
                const auto &event = input.get_keyboard_event(key);
                return event.press() || event.repeat();
            };
            const auto CTRL_PRESSED =
                is_key_pressed(Key::LEFT_CONTROL) || is_key_pressed(Key::RIGHT_CONTROL);

            // 处理模型旋转
            float sensitivity = 0.3f; // 降低灵敏度使旋转更平滑

            if (CTRL_PRESSED && mouse_left.press())
            {
                // Ctrl + 左键：只绕Z轴旋转
                // 只使用水平移动，忽略垂直移动
                float delta_angle = delta.x * sensitivity * 0.5f;

                // 创建绕Z轴旋转的四元数
                glm::quat rotation = glm::angleAxis(glm::radians(delta_angle),
                                                    glm::vec3(0.0f, 0.0f, 1.0f));

                // 应用旋转
                model_rotation_ = rotation * model_rotation_;
                model_rotation_ = glm::normalize(model_rotation_);

                std::println(
                    "[CameraController] Ctrl+Left mouse: Pure Z-axis rotation - "
                    "delta_x={:.1f}, angle={:.2f}°, vertical_delta_ignored={:.1f}",
                    delta.x, delta_angle, delta.y);
            }
            else if (mouse_left.press())
            {
                // 普通左键：绕X和Y轴旋转
                // 计算旋转角度
                float pitch_angle = -delta.y * sensitivity; // 上下移动控制俯仰
                float yaw_angle = delta.x * sensitivity;    // 左右移动控制偏航

                // 使用正确的旋转顺序避免万向锁
                glm::quat pitch_rotation = glm::angleAxis(glm::radians(pitch_angle),
                                                          glm::vec3(1.0f, 0.0f, 0.0f));
                glm::quat yaw_rotation =
                    glm::angleAxis(glm::radians(yaw_angle), glm::vec3(0.0f, 1.0f, 0.0f));

                // 应用旋转：先偏航后俯仰
                model_rotation_ = yaw_rotation * pitch_rotation * model_rotation_;
                model_rotation_ = glm::normalize(model_rotation_);

                std::println("[CameraController] Left mouse: X/Y-axis rotation - "
                             "delta({:.1f}, {:.1f}), pitch={:.2f}°, yaw={:.2f}°",
                             delta.x, delta.y, pitch_angle, yaw_angle);
            }
            // 处理相机旋转（右键）
            const auto &mouse_right =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_RIGHT);
            if (mouse_right.press() && glm::length(delta) > 0.0f)
            {
                float sensitivity = 0.1f; // 相机旋转灵敏度
                float yaw = -delta.x * sensitivity;
                float pitch = delta.y * sensitivity;

                camera_->rotate(yaw, pitch);

                std::println(
                    "[CameraController] Right mouse camera rotation: delta({:.1f}, "
                    "{:.1f}), yaw={:.2f}°, pitch={:.2f}°",
                    delta.x, delta.y, yaw, pitch);
            }

            // 处理相机平移（中键）
            const auto &mouse_middle =
                input.get_mousebutton_event(MouseButtons::MOUSE_BUTTON_MIDDLE);
            if (mouse_middle.press() && glm::length(delta) > 0.0f)
            {
                float pan_speed = 0.005f;
                float right_offset = -delta.x * pan_speed;
                float up_offset = delta.y * pan_speed;

                camera_->pan(right_offset, up_offset);

                std::println("[CameraController] Middle mouse panning: delta({:.1f}, "
                             "{:.1f}), right={:.3f}, up={:.3f}",
                             delta.x, delta.y, right_offset, up_offset);
            }

            last_mouse_pos_ = current_pos;
        }

        void handle_scroll(glfw_input &input)
        {
            using namespace mcs::vulkan::event;

            const auto &scroll = input.scroll();
            if (scroll != event::scroll_event{})
            {
                float zoom_factor = scroll.yoffset * 0.5f; // 缩放速度
                camera_->move(glm::vec3(0, 0, zoom_factor));

                std::println(
                    "[CameraController] Scrolled: yoffset={:.1f}, zoom_factor={:.2f}",
                    scroll.yoffset, zoom_factor);
            }
        }

      private:
        std::shared_ptr<Camera> camera_;
        bool enabled_ = true;

        // 鼠标状态
        glm::vec2 last_mouse_pos_ = glm::vec2(0.0f);

        // FIX: 模型旋转（使用四元数避免万向锁）
        glm::quat model_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };
} // namespace mcs::vulkan::camera

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

struct frame_context
{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    const logical_device *device_{};
    std::vector<VkSemaphore> presentCompleteSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::vector<VkFence> inFlightFences;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;

    explicit frame_context(const logical_device &device, size_t swapChainImagesSize)
        : device_{&device}
    {
        createSyncObjects(device.raw_data(), swapChainImagesSize);
    }

    ~frame_context() noexcept
    {
        destroy();
    }
    frame_context(const frame_context &) = delete;
    frame_context(frame_context &&) = delete;
    frame_context &operator=(const frame_context &) = delete;
    frame_context &operator=(frame_context &&) = delete;

  private:
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

    void destroySyncObject() noexcept
    {
        if (device_ != nullptr)
        {
            for (auto *semaphore : presentCompleteSemaphore)
                vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
            for (auto *semaphore : renderFinishedSemaphore)
                vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
            for (auto *fence : inFlightFences)
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

struct my_render
{
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

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "Camera Test";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr auto VERT_SHADER_PATH = "shaders/test_camera_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_camera_frag.spv";

struct alignas(16) CameraUBO
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

class Vertex
{
  public:
    glm::vec3 pos;
    glm::vec3 color;

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

    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

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
};

struct mesh_data
{
    physical_device &physicalDevice;
    logical_device &device;
    VkQueue queue;
    VkCommandPool commandPool;

    using index_type = uint32_t;
    std::vector<Vertex> vertices;
    std::vector<index_type> indices;

    struct FrameBuffers
    {
        mcs::vulkan::buffer_base vertexBuffer;
        mcs::vulkan::buffer_base indexBuffer;
    };
    std::array<FrameBuffers, MAX_FRAMES_IN_FLIGHT> frameBuffers;

    mesh_data(physical_device &physicalDevice, logical_device &device, VkQueue queue,
              VkCommandPool commandPool, std::vector<Vertex> vertices,
              std::vector<index_type> indices)
        : physicalDevice{physicalDevice}, device{device}, queue{queue},
          commandPool{commandPool}, vertices{std::move(vertices)},
          indices{std::move(indices)}
    {
        setupAllBuffers();
    }

    void setupAllBuffers()
    {
        for (auto &fb : frameBuffers)
        {
            createVertexBufferForFrame(fb);
            createIndexBufferForFrame(fb);
        }
    }

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

    void bind(VkCommandBuffer commandBuffer, uint32_t currentFrame) const noexcept
    {
        VkBuffer vertex = frameBuffers[currentFrame].vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, offsets);
        vkCmdBindIndexBuffer(commandBuffer,
                             frameBuffers[currentFrame].indexBuffer.buffer(), 0,
                             mapIndexType<index_type>());
    }

    void draw(VkCommandBuffer commandBuffer) const noexcept
    {
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0,
                         0);
    }

  private:
    void createVertexBufferForFrame(FrameBuffers &fb)
    {
        if (vertices.empty())
            return;

        const VkDeviceSize BUFFER_SIZE = sizeof(vertices[0]) * vertices.size();
        fb.vertexBuffer =
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = BUFFER_SIZE,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::staging_buffer stagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = BUFFER_SIZE,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        stagingBuffer.mapAndUnmapMempry(vertices.data(),
                                        static_cast<size_t>(BUFFER_SIZE));
        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 fb.vertexBuffer.buffer(), BUFFER_SIZE);
    }

    void createIndexBufferForFrame(FrameBuffers &fb)
    {
        if (indices.empty())
            return;

        const VkDeviceSize BUFFER_SIZE = sizeof(indices[0]) * indices.size();
        fb.indexBuffer = mcs::vulkan::create_buffer(
            physicalDevice, device,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = BUFFER_SIZE,
             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::staging_buffer stagingBuffer = mcs::vulkan::staging_buffer{
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = BUFFER_SIZE,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

        stagingBuffer.mapAndUnmapMempry(indices.data(), static_cast<size_t>(BUFFER_SIZE));
        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 fb.indexBuffer.buffer(), BUFFER_SIZE);
    }
};

struct UniformBufferManager
{
    std::vector<mcs::vulkan::buffer_base> uniformBuffers;
    std::vector<void *> mappedMemory;

    UniformBufferManager(physical_device &physicalDevice, logical_device &device,
                         size_t count)
    {
        uniformBuffers.resize(count);
        mappedMemory.resize(count);

        VkDeviceSize bufferSize = sizeof(CameraUBO);

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

    void updateBuffer(uint32_t index, const CameraUBO &ubo)
    {
        if (index < mappedMemory.size() && mappedMemory[index])
        {
            memcpy(mappedMemory[index], &ubo, sizeof(CameraUBO));
        }
    }
};

// NOTE: 相比：test_camera2。 旋转是绕屏幕的上下左右旋转的。无关模型的山下左右
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

        // diff: 创建相机和控制器
        using namespace mcs::vulkan::camera;
        auto camera = std::make_shared<Camera>();
        auto camera_controller = std::make_shared<CameraController>(camera);

        // 初始化相机
        camera->set_position(glm::vec3(0.0f, 0.0f, 5.0f));
        camera->set_perspective(45.0f, static_cast<float>(WIDTH) / HEIGHT, 0.1f, 100.0f);

        // 创建Vulkan实例
        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "Camera Test",
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

        // 创建交换链
        VkFormat swapChainImageFormat{};
        VkExtent2D swapChainExtent;
        VkSwapchainKHR swapChain{};
        std::vector<VkImage> swapChainImages{};

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
        createSwapChain();

        std::vector<VkImageView> swapChainImageViews;
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
                swapChainImageViews.emplace_back(
                    logical_device_.createImageView(imageViewCreateInfo, nullptr));
            }
        };
        createImageViews();

        // diff: 创建Uniform Buffer Manager
        UniformBufferManager uniformBufferManager(physicalDevice, logical_device_,
                                                  MAX_FRAMES_IN_FLIGHT);

        // 创建描述符池和布局
        VkDescriptorPool descriptorPool;
        VkDescriptorSetLayout descriptorSetLayout;

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
            VkDescriptorPoolSize poolSize{
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

            VkDescriptorPoolCreateInfo poolInfo{
                .sType = sType<VkDescriptorPoolCreateInfo>(),
                .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize};

            descriptorPool = logical_device_.createDescriptorPool(poolInfo);
        }

        std::vector<VkDescriptorSet> descriptorSets(MAX_FRAMES_IN_FLIGHT);
        {
            std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                       descriptorSetLayout);

            VkDescriptorSetAllocateInfo allocInfo{
                .sType = sType<VkDescriptorSetAllocateInfo>(),
                .descriptorPool = descriptorPool,
                .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
                .pSetLayouts = layouts.data()};

            descriptorSets = logical_device_.allocateDescriptorSets(allocInfo);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                VkDescriptorBufferInfo bufferInfo{
                    .buffer = uniformBufferManager.uniformBuffers[i].buffer(),
                    .offset = 0,
                    .range = sizeof(CameraUBO)};

                VkWriteDescriptorSet descriptorWrite{
                    .sType = sType<VkWriteDescriptorSet>(),
                    .dstSet = descriptorSets[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &bufferInfo};

                logical_device_.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
            }
        }

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
                .cullMode = VK_CULL_MODE_NONE, // NOTE: 没有剔除
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

            std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                         VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(),
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout};

            VkPipelineLayout pipelineLayout =
                logical_device_.createPipelineLayout(pipelineLayoutInfo, nullptr);

            structure_chain<VkGraphicsPipelineCreateInfo, VkPipelineRenderingCreateInfo>
                pipelineCreateInfoChain = {
                    {.stageCount = 2,
                     .pStages = shaderStages,
                     .pVertexInputState = &vertexInputInfo,
                     .pInputAssemblyState = &inputAssembly,
                     .pViewportState = &viewportState,
                     .pRasterizationState = &rasterizer,
                     .pMultisampleState = &multisampling,
                     .pColorBlendState = &colorBlending,
                     .pDynamicState = &dynamicState,
                     .layout = pipelineLayout,
                     .renderPass = VK_NULL_HANDLE},
                    {.colorAttachmentCount = 1,
                     .pColorAttachmentFormats = &swapChainImageFormat}};

            VkPipeline graphicsPipeline = logical_device_.createGraphicsPipelines(
                nullptr, 1, pipelineCreateInfoChain.head(), nullptr);

            return {pipelineLayout, graphicsPipeline};
        };

        auto [pipelineLayout, graphicsPipeline] = createGraphicsPipeline();

        // 创建命令池和缓冲区
        auto *commandPool = logical_device_.createCommandPool(
            {.sType = sType<VkCommandPoolCreateInfo>(),
             .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
             .queueFamilyIndex = graphicsIndex});

        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
        logical_device_.allocateCommandBuffers(
            commandBuffers[0],
            {.sType = sType<VkCommandBufferAllocateInfo>(),
             .commandPool = commandPool,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())});

        frame_context frameContext{logical_device_, swapChainImages.size()};

        auto cleanupSwapChain = [&]() {
            for (auto *imageView : swapChainImageViews)
                logical_device_.destroyImageView(imageView, nullptr);
            if (swapChain != nullptr)
                logical_device_.destroySwapchainKHR(swapChain);
            swapChainImageViews.clear();
            swapChain = nullptr;
        };

        auto recreateSwapChain = [&]() {
            window.waitGoodFramebufferSize();
            logical_device_.waitIdle();
            cleanupSwapChain();
            createSwapChain();
            createImageViews();
            // 更新相机宽高比
            auto [w, h] = window.getFramebufferSize();
            camera->set_window_size({w, h});
        };

        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame, mesh_data &input_mesh,
                                       uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];

            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};
            VkRenderingAttachmentInfo colorAttachment = {
                .sType = sType<VkRenderingAttachmentInfo>(),
                .imageView = imageView,
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = clearColor};

            VkRenderingInfo renderingInfo = {
                .sType = sType<VkRenderingInfo>(),
                .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment,
                .pDepthAttachment = nullptr};

            vkCmdBeginRendering(commandBuffer, &renderingInfo);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              graphicsPipeline);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1, &descriptorSets[currentFrame],
                                    0, nullptr);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            input_mesh.bind(commandBuffer, currentFrame);
            input_mesh.draw(commandBuffer);

            vkCmdEndRendering(commandBuffer);

            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        };

        // 创建3D立方体
        const std::vector<Vertex> vertices = {// 前面
                                              {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
                                              {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                              {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                              {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},

                                              // 后面
                                              {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                              {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                              {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
                                              {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}}};

        const std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0, // 前面
            4, 5, 6, 6, 7, 4, // 后面
            0, 3, 7, 7, 4, 0, // 左面
            1, 2, 6, 6, 5, 1, // 右面
            3, 2, 6, 6, 7, 3, // 上面
            0, 1, 5, 5, 4, 0  // 下面
        };

        mesh_data input_mesh{physicalDevice, logical_device_, graphicsQueue,
                             commandPool,    vertices,        indices};

        auto last_time = std::chrono::high_resolution_clock::now();
        float total_time = 0.0f;
        int frame_counter = 0;

        auto drawFrame = [&]() {
            auto &inFlightFences = frameContext.inFlightFences;
            auto &currentFrame = frameContext.currentFrame;
            auto &presentCompleteSemaphore = frameContext.presentCompleteSemaphore;
            auto &semaphoreIndex = frameContext.semaphoreIndex;
            auto &renderFinishedSemaphore = frameContext.renderFinishedSemaphore;

            auto *device = logical_device_.raw_data();

            // 等待上一帧完成
            while (vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                                   UINT64_MAX) == VK_TIMEOUT)
                ;

            // diff: 计算delta time并更新相机
            auto current_time = std::chrono::high_resolution_clock::now();
            float delta_time =
                std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;

            total_time += delta_time;
            frame_counter++;

            // 每秒打印一次帧率和相机状态
            if (total_time >= 1.0f)
            {
                float fps = frame_counter / total_time;
                auto pos = camera->get_position();
                auto forward = camera->get_forward_vector();
                auto yaw = camera->get_yaw();
                auto pitch = camera->get_pitch();

                // 获取模型旋转的欧拉角用于显示
                auto model_rotation = camera_controller->get_model_rotation();
                glm::vec3 euler = glm::eulerAngles(model_rotation) * glm::degrees(1.0f);

                std::println("\n[Frame] FPS: {:.1f}, Delta: {:.3f}ms", fps,
                             delta_time * 1000);
                std::println("[Camera] Pos({:.2f}, {:.2f}, {:.2f}), Yaw/Pitch({:.1f}, "
                             "{:.1f}), Forward({:.2f}, {:.2f}, {:.2f})",
                             pos.x, pos.y, pos.z, yaw, pitch, forward.x, forward.y,
                             forward.z);
                std::println("[Model] Rotation Euler: X={:.1f}, Y={:.1f}, Z={:.1f}",
                             euler.x, euler.y, euler.z);

                total_time = 0.0f;
                frame_counter = 0;
            }

            // 重置输入并轮询事件
            input.reset();
            surface::pollEvents();

            // diff: 更新相机控制器
            camera_controller->update(input, delta_time);

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

            // diff: 更新Uniform Buffer - 使用四元数创建模型矩阵
            CameraUBO ubo{};
            auto model_rotation = camera_controller->get_model_rotation();

            // 使用四元数创建模型矩阵
            ubo.model = glm::mat4_cast(model_rotation);

            ubo.view = camera->get_view_matrix();
            ubo.proj = camera->get_projection_matrix();

            uniformBufferManager.updateBuffer(currentFrame, ubo);

            recordCommandBuffer(commandBuffer, currentFrame, input_mesh, imageIndex);

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

        // 主循环
        std::println("\n=== Camera Test Started ===");
        std::println("Controls:");
        std::println("  WASD - Move forward/back/left/right");
        std::println("  QE   - Move up/down");
        std::println("  Ctrl - Slow down movement");
        std::println("  Left mouse button - Rotate model (X,Y axes)");
        std::println("  Ctrl + Left mouse button - Rotate model (Z axis)");
        std::println("  Right mouse button - Rotate camera");
        std::println("  Middle mouse button - Pan camera");
        std::println("  Mouse wheel - Zoom in/out");
        std::println("  R - Reset camera");
        std::println("  T - Reset model rotation");
        std::println("============================\n");
        std::println("Note: Model rotation sensitivity has been reduced.");
        std::println("      Mouse up = rotate up, Mouse right = rotate right\n");

        while (window.shouldClose() == 0)
        {
            drawFrame();
        }

        vkDeviceWaitIdle(logical_device_.raw_data());

        // 清理资源
        logical_device_.destroyPipeline(graphicsPipeline, nullptr);
        logical_device_.destroyPipelineLayout(pipelineLayout, nullptr);
        logical_device_.destroyDescriptorPool(descriptorPool);
        logical_device_.destroyDescriptorSetLayout(descriptorSetLayout);
        cleanupSwapChain();
        logical_device_.destroyCommandPool(commandPool);
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface_);

        std::println("\n=== Camera Test Finished ===");
    }
    catch (std::exception &e)
    {
        std::println("Error: {}", e.what());
    }

    return 0;
}