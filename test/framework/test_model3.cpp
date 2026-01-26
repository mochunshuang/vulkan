#include "./head.hpp"
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <print>
#include <utility>
#include <vector>
#include <memory>
#include <string>

#include <iomanip>

#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

using surface = mcs::vulkan::wsi::glfw::Window;

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

// NOLINTBEGIN
struct frame_context
{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    const logical_device *device_{};
    std::vector<VkSemaphore> presentCompleteSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
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
            if (::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                    &presentCompleteSemaphore[i]) != VK_SUCCESS ||
                ::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                    &renderFinishedSemaphore[i]) != VK_SUCCESS)

                throw std::runtime_error("failed to create semaphores!");
        }

        VkFenceCreateInfo fenceInfo = {.sType = sType<VkFenceCreateInfo>(),
                                       .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (::vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create fence!");
        }
    }

    constexpr void destroySyncObject() noexcept
    {
        for (auto *semaphore : presentCompleteSemaphore)
            ::vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
        presentCompleteSemaphore.clear();

        for (auto *semaphore : renderFinishedSemaphore)
            ::vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
        renderFinishedSemaphore.clear();

        for (auto *fence : inFlightFences)
            ::vkDestroyFence(device_->raw_data(), fence, nullptr);
    }

    constexpr void destroy() noexcept
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
        ::vkCmdPipelineBarrier2(commandBuffer, &dependency_info);
    }
};

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "2D/3D MVP 测试框架";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr auto VERT_SHADER_PATH = "shaders/test_model_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_model_frag.spv";
constexpr auto UI_VERT_SHADER_PATH = "shaders/test_ui_vert.spv"; // 2D UI顶点着色器
constexpr auto UI_FRAG_SHADER_PATH = "shaders/test_ui_frag.spv"; // 2D UI片元着色器

// ==================== 2D UI系统着色器示例 ====================

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

// ==================== 2D UI正交投影相机 ====================
/*
[2D UI坐标系]
屏幕空间（像素坐标）：
    (0,0) 左上角
    (width-1, height-1) 右下角
    Y轴向下（与Vulkan屏幕空间一致）

目标：将像素坐标映射到NDC [-1, 1]

正交投影矩阵公式：
    [ 2/width,     0,       0,  -1 ]
    [    0,    -2/height,   0,   1 ]  // 负号使Y轴向下
    [    0,         0,   1/(f-n), -n/(f-n) ]
    [    0,         0,       0,     1 ]

对于2D UI，我们通常使用：
    left = 0, right = width
    top = 0, bottom = height
    near = 0, far = 1
*/
class OrthographicCamera
{
  public:
    // ==================== 投影参数 ====================
    float left = 0.0f;     // 屏幕左边界（像素）
    float right = WIDTH;   // 屏幕右边界（像素）
    float bottom = HEIGHT; // 屏幕下边界（像素） - Vulkan Y向下
    float top = 0.0f;      // 屏幕上边界（像素）
    float near = 0.0f;     // 近平面
    float far = 1.0f;      // 远平面

    // ==================== 相机变换参数 ====================
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f); // 相机位置
    float rotation = 0.0f;                            // 旋转角度（弧度）
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);    // 缩放

    // ==================== 构造函数 ====================
    OrthographicCamera() = default;

    OrthographicCamera(float left, float right, float bottom, float top,
                       float near = 0.0f, float far = 1.0f)
        : left(left), right(right), bottom(bottom), top(top), near(near), far(far)
    {
    }

    // ==================== 投影矩阵（无旋转） ====================
    // 修复：正确的正交投影矩阵（Vulkan风格）
    glm::mat4 getProjectionMatrix() const
    {
        // Vulkan NDC坐标系：
        // X: 左(-1) → 右(+1)
        // Y: 上(-1) → 下(+1)  // 注意：与OpenGL相反！
        // Z: 近(0) → 远(1)     // 注意：与OpenGL的[-1,1]不同！

        // 正确的正交投影矩阵公式：
        // [ 2/(r-l),     0,         0,       -(r+l)/(r-l) ]
        // [    0,    -2/(b-t),       0,       -(b+t)/(b-t) ]  // 负号表示Y轴向下
        // [    0,         0,     1/(f-n),        -n/(f-n)  ]
        // [    0,         0,         0,             1      ]

        // 使用GLM，设置Y向下（Vulkan风格）
        return glm::ortho(left, right, bottom, top, 0.0f, 1.0f); // bottom > top
    }

    // ==================== 视图矩阵（包含旋转） ====================
    glm::mat4 getViewMatrix() const
    {
        glm::mat4 view = glm::mat4(1.0f);

        // 应用变换顺序：缩放 -> 旋转 -> 平移
        // 但因为是视图矩阵，我们要进行反向变换

        // 1. 平移（相机移动方向与物体相反）
        view = glm::translate(view, -position);

        // 2. 旋转（绕Z轴，2D旋转）
        if (rotation != 0.0f)
        {
            view = glm::rotate(view, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        // 3. 缩放
        view =
            glm::scale(view, glm::vec3(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z));

        return view;
    }

    // ==================== 视图投影矩阵 ====================
    glm::mat4 getViewProjectionMatrix() const
    {
        return getProjectionMatrix() * getViewMatrix();
    }

    // ==================== 设置屏幕尺寸 ====================
    void setScreenSize(float width, float height)
    {
        right = width;
        bottom = height;
    }

    // ==================== 变换控制方法 ====================

    // 移动相机
    void translate(const glm::vec2 &translation)
    {
        position.x += translation.x;
        position.y += translation.y;
    }

    void setPosition(const glm::vec2 &newPosition)
    {
        position.x = newPosition.x;
        position.y = newPosition.y;
    }

    // 旋转相机（2D旋转，绕Z轴）
    void rotate(float angleRadians)
    {
        rotation += angleRadians;
    }

    void setRotation(float angleRadians)
    {
        rotation = angleRadians;
    }

    // 缩放相机
    // 在OrthographicCamera类中添加/修改
    void zoom(float zoomFactor)
    {
        // 放大：zoomFactor > 1.0f
        // 缩小：zoomFactor < 1.0f
        scale *= zoomFactor;

        // 调整视口大小来实现缩放效果（更直观的方式）
        float centerX = (left + right) * 0.5f;
        float centerY = (top + bottom) * 0.5f;
        float newWidth = (right - left) / zoomFactor;
        float newHeight = (bottom - top) / zoomFactor;

        left = centerX - newWidth * 0.5f;
        right = centerX + newWidth * 0.5f;
        top = centerY - newHeight * 0.5f;
        bottom = centerY + newHeight * 0.5f;
    }

    void setZoom(float zoomLevel)
    {
        // 重置到原始尺寸再缩放
        left = 0.0f;
        right = WIDTH;
        top = 0.0f;
        bottom = HEIGHT;
        zoom(zoomLevel);
    }

    // 重置所有变换
    void resetTransform()
    {
        position = glm::vec3(0.0f);
        rotation = 0.0f;
        scale = glm::vec3(1.0f);
    }

    // ==================== 坐标变换方法 ====================

    // 像素坐标 → NDC（考虑相机变换）
    struct PixelToNDC
    {
        glm::vec2 pixel; // 像素坐标
        glm::vec2 ndc;   // NDC坐标
        glm::vec2 world; // 世界坐标
        bool isVisible;  // 是否在屏幕内
    };

    // 修复：正确的transformPixel函数
    PixelToNDC transformPixel(float pixelX, float pixelY) const
    {
        PixelToNDC result;
        result.pixel = glm::vec2(pixelX, pixelY);

        // 检查是否在屏幕内
        result.isVisible =
            (pixelX >= left && pixelX <= right && pixelY >= top && pixelY <= bottom);

        // 像素坐标 → NDC（Vulkan坐标系）
        // X: left→right 映射到 -1→1
        // Y: top→bottom 映射到 -1→1 (Y轴向下)
        float ndcX = 2.0f * (pixelX - left) / (right - left) - 1.0f;
        float ndcY = 2.0f * (pixelY - top) / (bottom - top) - 1.0f; // Y向下

        result.ndc = glm::vec2(ndcX, ndcY);

        // NDC → 世界坐标
        glm::vec4 clipPos(ndcX, ndcY, 0.0f, 1.0f);
        glm::mat4 inverseVP = glm::inverse(getViewProjectionMatrix());
        glm::vec4 worldPos = inverseVP * clipPos;

        if (worldPos.w != 0.0f)
        {
            worldPos /= worldPos.w;
        }

        result.world = glm::vec2(worldPos.x, worldPos.y);

        return result;
    }

    // 修复：正确的pixelToWorld函数
    glm::vec2 pixelToWorld(float pixelX, float pixelY) const
    {
        // 像素坐标 → NDC（Vulkan坐标系）
        float ndcX = 2.0f * (pixelX - left) / (right - left) - 1.0f;
        float ndcY = 2.0f * (pixelY - top) / (bottom - top) - 1.0f; // Y向下

        // NDC → 世界坐标
        glm::vec4 clipPos(ndcX, ndcY, 0.0f, 1.0f);
        glm::mat4 inverseVP = glm::inverse(getViewProjectionMatrix());
        glm::vec4 worldPos = inverseVP * clipPos;

        if (worldPos.w != 0.0f)
        {
            worldPos.x /= worldPos.w;
            worldPos.y /= worldPos.w;
        }

        return glm::vec2(worldPos.x, worldPos.y);
    }

    // 世界坐标 → 像素坐标
    glm::vec2 worldToPixel(const glm::vec2 &worldPos) const
    {
        // 世界坐标 → 裁剪空间
        glm::vec4 clipPos = getViewProjectionMatrix() * glm::vec4(worldPos, 0.0f, 1.0f);

        // 透视除法（正交投影中w=1）
        if (clipPos.w != 0.0f)
        {
            clipPos.x /= clipPos.w;
            clipPos.y /= clipPos.w;
        }

        // NDC → 像素坐标
        float pixelX = left + (clipPos.x + 1.0f) * 0.5f * (right - left);
        float pixelY = top + (1.0f - clipPos.y) * 0.5f * (bottom - top); // Y轴翻转

        return glm::vec2(pixelX, pixelY);
    }

    // 检查点是否在相机视野内（考虑旋转）
    bool isPointInView(const glm::vec2 &worldPoint, float margin = 0.0f) const
    {
        // 将世界点转换到相机局部空间
        glm::vec4 localPoint = getViewMatrix() * glm::vec4(worldPoint, 0.0f, 1.0f);

        return (localPoint.x >= left - margin && localPoint.x <= right + margin &&
                localPoint.y >= bottom - margin && localPoint.y <= top + margin);
    }

    // ==================== 创建特定UI区域的相机 ====================
    static OrthographicCamera createForUI(float x, float y, float width, float height)
    {
        return OrthographicCamera(x, x + width, y + height, y);
    }

    // ==================== 辅助方法 ====================

    // 获取相机朝向
    glm::vec2 getForward() const
    {
        return glm::vec2(cos(rotation), sin(rotation));
    }

    glm::vec2 getRight() const
    {
        return glm::vec2(-sin(rotation), cos(rotation));
    }

    // 获取视口中心
    glm::vec2 getViewportCenter() const
    {
        return glm::vec2(left + (right - left) * 0.5f, top + (bottom - top) * 0.5f);
    }

    // 获取视口大小
    glm::vec2 getViewportSize() const
    {
        return glm::vec2(right - left, bottom - top);
    }

    // 设置视口（保持宽高比）
    void setViewport(float width, float height, bool keepAspectRatio = true,
                     float targetAspectRatio = 16.0f / 9.0f)
    {
        if (keepAspectRatio)
        {
            float currentAspect = width / height;
            if (currentAspect > targetAspectRatio)
            {
                // 太宽，调整高度
                float newHeight = width / targetAspectRatio;
                float centerY = (top + bottom) * 0.5f;
                top = centerY - newHeight * 0.5f;
                bottom = centerY + newHeight * 0.5f;
                right = left + width;
            }
            else
            {
                // 太高，调整宽度
                float newWidth = height * targetAspectRatio;
                float centerX = (left + right) * 0.5f;
                left = centerX - newWidth * 0.5f;
                right = centerX + newWidth * 0.5f;
                bottom = top + height;
            }
        }
        else
        {
            right = left + width;
            bottom = top + height;
        }
    }

    // 更新相机，使其包含特定区域
    void fitToBounds(const glm::vec2 &minBounds, const glm::vec2 &maxBounds,
                     float padding = 0.1f)
    {
        // 计算边界框
        glm::vec2 boundsSize = maxBounds - minBounds;
        glm::vec2 boundsCenter = (minBounds + maxBounds) * 0.5f;

        // 设置相机位置
        setPosition(boundsCenter);

        // 计算需要的缩放
        glm::vec2 viewportSize = getViewportSize();
        float scaleX = boundsSize.x * (1.0f + padding) / viewportSize.x;
        float scaleY = boundsSize.y * (1.0f + padding) / viewportSize.y;
        float requiredScale = glm::max(scaleX, scaleY);

        // 设置缩放
        setZoom(1.0f / requiredScale);
    }
};

// ==================== 3D透视投影相机 ====================
class QuaternionCamera
{
  public:
    // ==================== 世界空间中的相机参数 ====================
    glm::vec3 position = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    // ==================== 投影参数 ====================
    float fov = 45.0f;
    float aspectRatio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // ==================== 构造函数 ====================
    QuaternionCamera(const glm::vec3 &target = glm::vec3(0, 0, 0),
                     const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        lookAt(target, up);
    }

    // ==================== 视图矩阵 V ====================
    glm::mat4 getViewMatrix() const
    {
        // 视图矩阵是相机变换的逆
        // 对于旋转R和平移t，视图矩阵的旋转部分是R的逆（即共轭），平移部分是-R的逆 * t
        glm::mat4 rotationMatrix = glm::mat4_cast(glm::conjugate(rotation));
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -position);
        // 注意：由于旋转矩阵是正交矩阵，其逆等于转置，所以用共轭构造的旋转矩阵就是逆。
        // 视图矩阵 = 旋转的逆 * 平移的逆，即先平移回去，再旋转回去。
        return rotationMatrix * translationMatrix;
    }

    // ==================== 投影矩阵 P ====================
    glm::mat4 getProjectionMatrix() const
    {
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }

    // ==================== 辅助函数 ====================
    glm::vec3 getForward() const
    {
        return rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    }
    glm::vec3 getRight() const
    {
        return rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    }
    glm::vec3 getUp() const
    {
        return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    }

    void lookAt(const glm::vec3 &target,
                const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        glm::vec3 desiredForward = glm::normalize(target - position);
        glm::vec3 desiredUp = glm::normalize(up);
        glm::vec3 right = glm::normalize(glm::cross(desiredForward, desiredUp));
        desiredUp = glm::normalize(glm::cross(right, desiredForward));

        glm::mat3 rotationMat;
        rotationMat[0] = right;
        rotationMat[1] = desiredUp;
        rotationMat[2] = -desiredForward;

        rotation = glm::quat_cast(rotationMat);
    }

    /*
    Pitch（俯仰） - 绕X轴旋转 - 飞机抬头/低头
    Yaw（偏航） - 绕Y轴旋转 - 飞机左右转向
    Roll（滚转） - 绕Z轴旋转 - 飞机侧倾/滚转
    */
    void rotatePitch(float angleDegrees)
    {
        glm::quat pitchRotation = glm::angleAxis(glm::radians(angleDegrees), getRight());
        rotation = pitchRotation * rotation;
    }
    void rotateYaw(float angleDegrees)
    {
        glm::quat yawRotation =
            glm::angleAxis(glm::radians(angleDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = yawRotation * rotation;
    }
    void rotateRoll(float angleDegrees)
    {
        glm::quat rollRotation = glm::angleAxis(glm::radians(angleDegrees), getForward());
        rotation = rollRotation * rotation;
    }

    void moveForward(float distance)
    {
        position += getForward() * distance;
    }
    void moveRight(float distance)
    {
        position += getRight() * distance;
    }
    void moveUp(float distance)
    {
        position += getUp() * distance;
    }

    void updateAspectRatio(float width, float height)
    {
        aspectRatio = width / height;
    }
};

// ==================== 变换组件 ====================
struct Transform
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::identity<glm::quat>();
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 getModelMatrix() const
    {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMat = glm::mat4_cast(rotation);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
        return translation * rotationMat * scaleMat;
    }

    void setPosition(const glm::vec3 &pos)
    {
        position = pos;
    }
    void setRotation(const glm::quat &rot)
    {
        rotation = rot;
    }
    void setScale(const glm::vec3 &scl)
    {
        scale = scl;
    }
};

// ==================== 渲染对象基类 ====================
class RenderObject
{
  public:
    Transform transform;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool is2D = false; // 标识是否为2D对象

    RenderObject(std::vector<Vertex> verts, std::vector<uint32_t> idxs, bool is2D = false)
        : vertices(std::move(verts)), indices(std::move(idxs)), is2D(is2D)
    {
    }

    // 3D MVP计算
    glm::mat4 calculateMVP(const QuaternionCamera &camera) const
    {
        glm::mat4 model = transform.getModelMatrix();
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = camera.getProjectionMatrix();
        return proj * view * model;
    }

    // 2D MVP计算
    glm::mat4 calculateMVP(const OrthographicCamera &camera) const
    {
        // 对于2D UI，模型矩阵可能包含位置偏移和缩放
        // 但通常我们只需要正交投影矩阵
        glm::mat4 model = transform.getModelMatrix();
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = camera.getProjectionMatrix();
        return proj * view * model;
    }
};

// ==================== 场景管理器 ====================
class Scene
{
  private:
    std::vector<RenderObject> objects3D; // 3D对象
    std::vector<RenderObject> objects2D; // 2D UI对象
    QuaternionCamera camera3D;           // 3D相机
    OrthographicCamera camera2D;         // 2D UI相机

  public:
    void addObject3D(RenderObject &&obj)
    {
        objects3D.push_back(std::move(obj));
    }
    void addObject2D(RenderObject &&obj)
    {
        objects2D.push_back(std::move(obj));
    }

    const std::vector<RenderObject> &getObjects3D() const
    {
        return objects3D;
    }
    const std::vector<RenderObject> &getObjects2D() const
    {
        return objects2D;
    }

    QuaternionCamera &getCamera3D()
    {
        return camera3D;
    }
    const QuaternionCamera &getCamera3D() const
    {
        return camera3D;
    }

    OrthographicCamera &getCamera2D()
    {
        return camera2D;
    }
    const OrthographicCamera &getCamera2D() const
    {
        return camera2D;
    }

    void clearObjects()
    {
        objects3D.clear();
        objects2D.clear();
    }
    size_t objectCount3D() const
    {
        return objects3D.size();
    }
    size_t objectCount2D() const
    {
        return objects2D.size();
    }
};

// ==================== Uniform Buffer对象 ====================
struct UniformBufferObject
{
    alignas(16) glm::mat4 mvp; // MVP矩阵
};

// ==================== 网格数据管理器 ====================
class MeshData
{
  private:
    physical_device &physicalDevice;
    logical_device &device;
    VkQueue queue;
    VkCommandPool commandPool;

    struct FrameBuffer
    {
        mcs::vulkan::buffer_base vertexBuffer;
        mcs::vulkan::buffer_base indexBuffer;
        mcs::vulkan::buffer_base uniformBuffer;
    };
    std::array<FrameBuffer, MAX_FRAMES_IN_FLIGHT> frameBuffers;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;

    bool is2D; // 是否为2D对象

  public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    MeshData(physical_device &physicalDevice, logical_device &device, VkQueue queue,
             VkCommandPool commandPool, const std::vector<Vertex> &vertices,
             const std::vector<uint32_t> &indices, bool is2D = false)
        : physicalDevice(physicalDevice), device(device), queue(queue),
          commandPool(commandPool), vertices(vertices), indices(indices), is2D(is2D)
    {
        createDescriptorSetLayout();
        createDescriptorPool();
        createBuffers();
        createDescriptorSets();
    }

    ~MeshData()
    {
        vkDestroyDescriptorPool(device.raw_data(), descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device.raw_data(), descriptorSetLayout, nullptr);
    }

    // 更新Uniform Buffer
    void updateUniformBuffer(uint32_t currentFrame, const glm::mat4 &mvp)
    {
        UniformBufferObject ubo{};
        ubo.mvp = mvp;

        auto &uniformBuffer = frameBuffers[currentFrame].uniformBuffer;
        void *data;
        vkMapMemory(device.raw_data(), uniformBuffer.bufferMemory(), 0,
                    sizeof(UniformBufferObject), 0, &data);
        memcpy(data, &ubo, sizeof(UniformBufferObject));
        vkUnmapMemory(device.raw_data(), uniformBuffer.bufferMemory());
    }

    void bind(VkCommandBuffer commandBuffer, uint32_t currentFrame) const
    {
        VkBuffer vertexBuffer = frameBuffers[currentFrame].vertexBuffer.buffer();
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

        vkCmdBindIndexBuffer(commandBuffer,
                             frameBuffers[currentFrame].indexBuffer.buffer(), 0,
                             VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0,
                                nullptr);
    }

    void draw(VkCommandBuffer commandBuffer) const
    {
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0,
                         0);
    }

    VkDescriptorSetLayout getDescriptorSetLayout() const
    {
        return descriptorSetLayout;
    }

    static VkPipelineLayout pipelineLayout;
    static VkPipelineLayout pipelineLayout2D; // 2D UI管线布局

  private:
    void createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(device.raw_data(), &layoutInfo, nullptr,
                                        &descriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void createDescriptorPool()
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device.raw_data(), &poolInfo, nullptr,
                                   &descriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                   descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(device.raw_data(), &allocInfo,
                                     descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = frameBuffers[i].uniformBuffer.buffer();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device.raw_data(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    void createBuffers()
    {
        for (auto &fb : frameBuffers)
        {
            createVertexBuffer(fb);
            createIndexBuffer(fb);
            createUniformBuffer(fb);
        }
    }

    void createVertexBuffer(FrameBuffer &fb)
    {
        if (vertices.empty())
            return;

        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        auto stagingBuffer = mcs::vulkan::staging_buffer(mcs::vulkan::create_buffer(
            physicalDevice, device,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = bufferSize,
             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        stagingBuffer.mapAndUnmapMemory(vertices.data(), bufferSize);

        fb.vertexBuffer =
            mcs::vulkan::create_buffer(physicalDevice, device,
                                       {.sType = sType<VkBufferCreateInfo>(),
                                        .size = bufferSize,
                                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 fb.vertexBuffer.buffer(), bufferSize);
    }

    void createIndexBuffer(FrameBuffer &fb)
    {
        if (indices.empty())
            return;

        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        auto stagingBuffer = mcs::vulkan::staging_buffer(mcs::vulkan::create_buffer(
            physicalDevice, device,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = bufferSize,
             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        stagingBuffer.mapAndUnmapMemory(indices.data(), bufferSize);

        fb.indexBuffer = mcs::vulkan::create_buffer(
            physicalDevice, device,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = bufferSize,
             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mcs::vulkan::copy_buffer(device, queue, commandPool, stagingBuffer.buffer(),
                                 fb.indexBuffer.buffer(), bufferSize);
    }

    void createUniformBuffer(FrameBuffer &fb)
    {
        fb.uniformBuffer = mcs::vulkan::create_buffer(
            physicalDevice, device,
            {.sType = sType<VkBufferCreateInfo>(),
             .size = sizeof(UniformBufferObject),
             .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
             .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
};

// 静态管线布局变量
VkPipelineLayout MeshData::pipelineLayout = nullptr;
VkPipelineLayout MeshData::pipelineLayout2D = nullptr;

// ==================== API测试函数（静态测试） ====================
/*
[静态MVP测试用例]
这些测试函数可以在程序启动时运行，验证MVP矩阵计算的正确性
注意：这些测试不涉及实际的Vulkan渲染，只测试数学计算
*/

// ==================== 改进的静态测试函数 ====================

// 辅助函数：打印矩阵
void printMatrix(const std::string &name, const glm::mat4 &mat)
{
    std::cout << name << ":\n";
    for (int i = 0; i < 4; ++i)
    {
        std::cout << "  ";
        for (int j = 0; j < 4; ++j)
        {
            std::cout << std::setw(10) << std::fixed << std::setprecision(4) << mat[i][j]
                      << " ";
        }
        std::cout << std::endl;
    }
}

void printMatrix3x3(const std::string &name, const glm::mat3 &mat)
{
    std::cout << name << ":\n";
    for (int i = 0; i < 3; ++i)
    {
        std::cout << "  ";
        for (int j = 0; j < 3; ++j)
        {
            std::cout << std::setw(10) << std::fixed << std::setprecision(4) << mat[i][j]
                      << " ";
        }
        std::cout << std::endl;
    }
}

// 测试1：3D透视投影测试
void testPerspectiveMVP()
{
    std::cout << "\n=== 3D透视投影MVP测试 ===" << std::endl;

    QuaternionCamera camera;
    camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
    camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));

    Transform transform;
    transform.setPosition(glm::vec3(1.0f, 2.0f, 3.0f));
    transform.setRotation(
        glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    transform.setScale(glm::vec3(2.0f, 1.0f, 0.5f));

    // 计算各个矩阵
    glm::mat4 model = transform.getModelMatrix();
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    glm::mat4 mvp = proj * view * model;

    printMatrix("模型矩阵M", model);
    printMatrix("视图矩阵V", view);
    printMatrix("投影矩阵P", proj);
    printMatrix("MVP矩阵", mvp);

    // 测试一个顶点
    glm::vec4 vertex(0.0f, 0.0f, 0.0f, 1.0f); // 模型空间原点
    glm::vec4 worldPos = model * vertex;
    glm::vec4 viewPos = view * worldPos;
    glm::vec4 clipPos = proj * viewPos;

    std::cout << "\n顶点变换测试:" << std::endl;
    std::cout << "模型空间: (" << vertex.x << ", " << vertex.y << ", " << vertex.z << ")"
              << std::endl;
    std::cout << "世界空间: (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z
              << ")" << std::endl;
    std::cout << "视图空间: (" << viewPos.x << ", " << viewPos.y << ", " << viewPos.z
              << ")" << std::endl;
    std::cout << "裁剪空间: (" << clipPos.x << ", " << clipPos.y << ", " << clipPos.z
              << ", w=" << clipPos.w << ")" << std::endl;

    // 验证MVP计算结果一致
    glm::vec4 clipPos2 = mvp * vertex;
    std::cout << "MVP直接计算: (" << clipPos2.x << ", " << clipPos2.y << ", "
              << clipPos2.z << ", w=" << clipPos2.w << ")" << std::endl;

    float diff = glm::distance(clipPos, clipPos2);
    std::cout << "计算结果差异: " << diff << " (应接近0)" << std::endl;
}

// 测试2：2D正交投影测试
void testOrthographicMVP()
{
    std::cout << "\n=== 2D正交投影MVP测试 ===" << std::endl;

    OrthographicCamera camera;
    camera.setScreenSize(800.0f, 600.0f);

    // 测试投影矩阵
    glm::mat4 proj = camera.getProjectionMatrix();
    printMatrix("正交投影矩阵P", proj);

    // 测试像素到NDC的变换
    auto testPixel = [&](float x, float y, const std::string &desc) {
        auto result = camera.transformPixel(x, y);
        std::cout << "\n" << desc << ":" << std::endl;
        std::cout << "  像素坐标: (" << result.pixel.x << ", " << result.pixel.y << ")"
                  << std::endl;
        std::cout << "  NDC坐标: (" << result.ndc.x << ", " << result.ndc.y << ")"
                  << std::endl;
        std::cout << "  世界坐标: (" << result.world.x << ", " << result.world.y << ")"
                  << std::endl;
        std::cout << "  是否可见: " << (result.isVisible ? "是" : "否") << std::endl;

        // 验证往返变换
        glm::vec2 pixel2 = camera.worldToPixel(result.world);
        float diff = glm::distance(result.pixel, pixel2);
        std::cout << "  往返变换误差: " << diff << std::endl;
    };

    testPixel(0.0f, 0.0f, "左上角");
    testPixel(400.0f, 300.0f, "中心");
    testPixel(800.0f, 600.0f, "右下角");
    testPixel(-10.0f, -10.0f, "屏幕外左上");
    testPixel(810.0f, 610.0f, "屏幕外右下");
}

// 测试3：四元数旋转测试
// ==================== 修复后的四元数测试 ====================
void testQuaternionRotation()
{
    std::cout << "\n=== 四元数旋转测试 ===" << std::endl;

    Transform transform;

    // 测试绕Y轴旋转90度 - 修正期望值
    transform.setRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)));
    glm::mat4 modelX = transform.getModelMatrix();

    // 提取3x3旋转部分
    glm::mat3 rotMatX = glm::mat3(modelX);
    printMatrix3x3("绕Y轴90度旋转矩阵", rotMatX);

    // 验证旋转是否正确
    glm::vec3 testVec(0.0f, 0.0f, -1.0f); // 前向向量
    glm::vec3 rotatedVec = rotMatX * testVec;

    std::cout << "\n向量(0,0,-1)绕Y轴旋转90度:" << std::endl;
    std::cout << "  矩阵旋转: (" << rotatedVec.x << ", " << rotatedVec.y << ", "
              << rotatedVec.z << ")" << std::endl;

    // 修正：绕Y轴旋转90度，前向应该变成(-1,0,0)
    std::cout << "  应接近: (-1, 0, 0)" << std::endl;

    // 使用四元数验证
    glm::quat rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    glm::vec3 rotatedByQuat = rotation * testVec;
    std::cout << "  四元数旋转: (" << rotatedByQuat.x << ", " << rotatedByQuat.y << ", "
              << rotatedByQuat.z << ")" << std::endl;
}

// 测试4：深度范围测试
void testDepthRange()
{
    std::cout << "\n=== 深度范围测试 ===" << std::endl;

    QuaternionCamera camera;
    camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
    camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    camera.nearPlane = 0.1f;
    camera.farPlane = 100.0f;

    std::vector<std::pair<glm::vec3, std::string>> testPoints = {
        {glm::vec3(0.0f, 0.0f, 4.9f), "近平面前(非常近)"},
        {glm::vec3(0.0f, 0.0f, 5.1f), "近平面后"},
        {glm::vec3(0.0f, 0.0f, 4.0f), "近远平面之间"},
        {glm::vec3(0.0f, 0.0f, 0.0f), "焦点位置"},
        {glm::vec3(0.0f, 0.0f, -50.0f), "相机前方远处"},
        {glm::vec3(0.0f, 0.0f, -95.0f), "接近远平面"},
        {glm::vec3(0.0f, 0.0f, -105.0f), "远平面后"}};

    for (const auto &[point, desc] : testPoints)
    {
        glm::vec4 viewPos = camera.getViewMatrix() * glm::vec4(point, 1.0f);
        glm::vec4 clipPos = camera.getProjectionMatrix() * viewPos;

        bool isVisible = true;
        std::string reason = "";

        // 检查w分量（透视除法前）
        if (clipPos.w <= 0.0f)
        {
            isVisible = false;
            reason = "w<=0 (在相机后方)";
        }
        else
        {
            // 执行透视除法
            glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

            // 检查NDC坐标是否在[-1, 1]范围内
            bool inNDC = (ndc.x >= -1.0f && ndc.x <= 1.0f && ndc.y >= -1.0f &&
                          ndc.y <= 1.0f && ndc.z >= 0.0f && ndc.z <= 1.0f);

            if (!inNDC)
            {
                isVisible = false;
                if (ndc.z < 0.0f)
                    reason = "Z<0 (近平面前)";
                else if (ndc.z > 1.0f)
                    reason = "Z>1 (远平面后)";
                else if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f)
                    reason = "XY超出视锥体";
            }
        }

        std::cout << desc << " 世界(" << point.z << "): ";
        std::cout << "视图Z=" << viewPos.z << ", ";
        std::cout << "裁剪w=" << clipPos.w << ", ";
        std::cout << " → " << (isVisible ? "可见" : "不可见");
        if (!reason.empty())
            std::cout << " [" << reason << "]";
        std::cout << std::endl;
    }
}

// 测试5：矩阵乘法顺序验证
void testMatrixMultiplicationOrder()
{
    std::cout << "\n=== 矩阵乘法顺序验证 ===" << std::endl;

    // 创建平移和旋转矩阵
    glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(1, 0, 0));
    glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));

    printMatrix("平移矩阵T", T);
    printMatrix("旋转矩阵R", R);

    glm::vec4 point(0, 0, 0, 1);

    // 测试不同的乘法顺序
    glm::vec4 resultTR = T * R * point; // 先旋转后平移
    glm::vec4 resultRT = R * T * point; // 先平移后旋转

    std::cout << "\n点(0,0,0)变换结果:" << std::endl;
    std::cout << "先旋转后平移 (T*R): (" << resultTR.x << ", " << resultTR.y << ", "
              << resultTR.z << ")" << std::endl;
    std::cout << "先平移后旋转 (R*T): (" << resultRT.x << ", " << resultRT.y << ", "
              << resultRT.z << ")" << std::endl;

    // 验证：对于模型矩阵，正确的顺序是 平移 * 旋转 * 缩放
    Transform transform;
    transform.setPosition(glm::vec3(1, 0, 0));
    transform.setRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0)));
    transform.setScale(glm::vec3(2, 1, 1));

    glm::mat4 model = transform.getModelMatrix();
    glm::vec4 modelResult = model * point;
    std::cout << "模型矩阵变换 (平移*旋转*缩放): (" << modelResult.x << ", "
              << modelResult.y << ", " << modelResult.z << ")" << std::endl;

    // 验证模型矩阵分解
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(model, scale, rotation, translation, skew, perspective);

    std::cout << "\n模型矩阵分解:" << std::endl;
    std::cout << "  平移: (" << translation.x << ", " << translation.y << ", "
              << translation.z << ")" << std::endl;
    std::cout << "  旋转角度: " << glm::degrees(glm::angle(rotation)) << "度"
              << std::endl;
    std::cout << "  缩放: (" << scale.x << ", " << scale.y << ", " << scale.z << ")"
              << std::endl;
}

// 测试6：2D相机变换验证
void test2DCameraTransform()
{
    std::cout << "\n=== 2D相机变换验证 ===" << std::endl;

    OrthographicCamera camera;
    camera.setScreenSize(800.0f, 600.0f);

    // 测试1：平移
    camera.setPosition(glm::vec2(100, 50));
    auto result1 = camera.transformPixel(0, 0);
    std::cout << "相机平移(100,50)后，屏幕原点(0,0):" << std::endl;
    std::cout << "  NDC: (" << result1.ndc.x << ", " << result1.ndc.y << ")" << std::endl;
    std::cout << "  世界坐标: (" << result1.world.x << ", " << result1.world.y << ")"
              << std::endl;

    // 测试2：旋转
    camera.resetTransform();
    camera.setRotation(glm::radians(45.0f));
    glm::vec2 worldPoint(400, 300);
    glm::vec2 pixelPos = camera.worldToPixel(worldPoint);
    std::cout << "\n旋转45度后，世界点(400,300):" << std::endl;
    std::cout << "  像素坐标: (" << pixelPos.x << ", " << pixelPos.y << ")" << std::endl;

    // 验证往返变换
    glm::vec2 worldPos2 = camera.pixelToWorld(pixelPos.x, pixelPos.y);
    float diff = glm::distance(worldPoint, worldPos2);
    std::cout << "  往返变换误差: " << diff << std::endl;

    // 测试3：缩放
    camera.resetTransform();
    camera.setZoom(2.0f);
    auto result3 = camera.transformPixel(400, 300);
    std::cout << "\n缩放2倍后，屏幕中心(400,300):" << std::endl;
    std::cout << "  NDC: (" << result3.ndc.x << ", " << result3.ndc.y << ")" << std::endl;
    std::cout << "  视口大小: " << camera.getViewportSize().x << " x "
              << camera.getViewportSize().y << std::endl;
}

// 测试7：相机变换链一致性验证
void testCameraTransformationChain()
{
    std::cout << "\n=== 相机变换链一致性验证 ===" << std::endl;

    // 测试3D相机
    QuaternionCamera camera3D;
    camera3D.position = glm::vec3(1, 2, 5);
    camera3D.lookAt(glm::vec3(0, 0, 0));

    // 计算视图矩阵
    glm::mat4 viewMatrix = camera3D.getViewMatrix();

    // 验证视图矩阵的正确性：将相机位置变换到视图空间应为(0,0,0)
    glm::vec4 cameraPosInView = viewMatrix * glm::vec4(camera3D.position, 1.0f);
    std::cout << "3D相机位置(" << camera3D.position.x << ", " << camera3D.position.y
              << ", " << camera3D.position.z << ")" << std::endl;
    std::cout << "在视图空间中应为(0,0,?): (" << cameraPosInView.x << ", "
              << cameraPosInView.y << ", " << cameraPosInView.z << ")" << std::endl;

    // 验证前向向量
    glm::vec3 forward = camera3D.getForward();
    glm::vec3 toTarget = glm::normalize(glm::vec3(0, 0, 0) - camera3D.position);
    float dot = glm::dot(forward, toTarget);
    std::cout << "相机前向与目标方向点积: " << dot << " (应接近1)" << std::endl;

    // 测试2D相机
    OrthographicCamera camera2D;
    camera2D.setScreenSize(800, 600);
    camera2D.setPosition(glm::vec2(100, 50));
    camera2D.setRotation(glm::radians(30.0f));

    // 验证2D相机变换
    glm::mat4 vp2D = camera2D.getViewProjectionMatrix();
    glm::vec4 testPoint2D(400, 300, 0, 1);
    glm::vec4 transformed2D = vp2D * testPoint2D;

    std::cout << "\n2D测试点(400,300,0)变换后: (" << transformed2D.x << ", "
              << transformed2D.y << ", " << transformed2D.z << ", w=" << transformed2D.w
              << ")" << std::endl;
}

// 测试8：MVP矩阵可逆性验证
void testMVPInverse()
{
    std::cout << "\n=== MVP矩阵可逆性验证 ===" << std::endl;

    QuaternionCamera camera;
    camera.position = glm::vec3(0, 0, 5);
    camera.lookAt(glm::vec3(0, 0, 0));

    Transform transform;
    transform.setPosition(glm::vec3(1, 2, 3));
    transform.setRotation(glm::quat(1, 0, 0, 0));
    transform.setScale(glm::vec3(1, 1, 1));

    // 计算MVP
    glm::mat4 model = transform.getModelMatrix();
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    glm::mat4 mvp = proj * view * model;

    // 检查行列式
    float detMVP = glm::determinant(mvp);
    float detModel = glm::determinant(model);
    float detView = glm::determinant(view);
    float detProj = glm::determinant(proj);

    std::cout << "行列式值:" << std::endl;
    std::cout << "  模型矩阵: " << detModel << " (应为1，因为是刚体变换)" << std::endl;
    std::cout << "  视图矩阵: " << detView << " (应为1，因为是刚体变换)" << std::endl;
    std::cout << "  投影矩阵: " << detProj << " (应不为0)" << std::endl;
    std::cout << "  MVP矩阵: " << detMVP << " (应不为0)" << std::endl;

    // 测试可逆性：选择一个点，进行正向和逆向变换
    if (std::abs(detMVP) > 1e-6f)
    {
        glm::mat4 inverseMVP = glm::inverse(mvp);

        // 测试点：模型空间中的点
        glm::vec4 modelPoint(0.5f, 0.5f, 0.5f, 1.0f);
        glm::vec4 clipPoint = mvp * modelPoint;
        glm::vec4 modelPointRestored = inverseMVP * clipPoint;

        // 透视除法
        if (modelPointRestored.w != 0.0f)
        {
            modelPointRestored /= modelPointRestored.w;
        }

        std::cout << "\n可逆性测试:" << std::endl;
        std::cout << "  原始模型点: (" << modelPoint.x << ", " << modelPoint.y << ", "
                  << modelPoint.z << ")" << std::endl;
        std::cout << "  裁剪空间点: (" << clipPoint.x << ", " << clipPoint.y << ", "
                  << clipPoint.z << ", w=" << clipPoint.w << ")" << std::endl;
        std::cout << "  恢复的模型点: (" << modelPointRestored.x << ", "
                  << modelPointRestored.y << ", " << modelPointRestored.z << ")"
                  << std::endl;

        float error = glm::distance(glm::vec3(modelPoint), glm::vec3(modelPointRestored));
        std::cout << "  误差: " << error << " (应接近0)" << std::endl;
    }
    else
    {
        std::cout << "警告：MVP矩阵不可逆！" << std::endl;
    }
}

// C0: 左右手坐标系. 差个-号 是没办法的
//  测试9：四元数方向一致性测试
void testQuaternionDirectionConsistency()
{
    std::cout << "\n=== 四元数方向一致性测试 ===" << std::endl;

    {
        Transform transform;

        // 测试绕Y轴90度
        transform.setRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)));

        // 方法1：使用矩阵验证
        glm::mat4 rotationMat = glm::mat4_cast(transform.rotation);
        glm::vec3 forward(0, 0, -1);
        glm::vec3 rotatedByMatrix = glm::vec3(rotationMat * glm::vec4(forward, 0.0f));

        std::cout << "矩阵旋转结果: (" << rotatedByMatrix.x << ", " << rotatedByMatrix.y
                  << ", " << rotatedByMatrix.z << ")" << std::endl;

        // 方法2：手动计算期望值
        // 绕Y轴旋转矩阵：
        // [ cosθ, 0, sinθ]
        // [  0,   1,   0 ]
        // [-sinθ, 0, cosθ]
        float θ = glm::radians(90.0f);
        glm::mat3 manualRot = glm::mat3(cos(θ), 0, sin(θ), 0, 1, 0, -sin(θ), 0, cos(θ));
        glm::vec3 manualResult = manualRot * glm::vec3(0, 0, -1);
        std::cout << "手动计算期望: (" << manualResult.x << ", " << manualResult.y << ", "
                  << manualResult.z << ")" << std::endl
                  << std::endl;
    }

    // 测试绕不同轴旋转应该保持右手坐标系
    Transform transform;

    // 测试1：绕X轴90度
    transform.setRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0)));
    glm::vec3 up(0, 1, 0);
    glm::vec3 rotatedUp = transform.rotation * up;
    std::cout << "绕X轴90度旋转后up向量: (" << rotatedUp.x << ", " << rotatedUp.y << ", "
              << rotatedUp.z << ")" << std::endl;
    std::cout << "应接近: (0, 0, 1)" << std::endl;

    // 测试2：绕Y轴90度
    transform.setRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)));
    glm::vec3 forward(0, 0, -1);
    glm::vec3 rotatedForward = transform.rotation * forward;
    std::cout << "\n绕Y轴90度旋转后forward向量: (" << rotatedForward.x << ", "
              << rotatedForward.y << ", " << rotatedForward.z << ")" << std::endl;
    std::cout << "应接近: (1, 0, 0)" << std::endl;

    // 测试3：绕Z轴90度
    transform.setRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1)));
    glm::vec3 right(1, 0, 0);
    glm::vec3 rotatedRight = transform.rotation * right;
    std::cout << "\n绕Z轴90度旋转后right向量: (" << rotatedRight.x << ", "
              << rotatedRight.y << ", " << rotatedRight.z << ")" << std::endl;
    std::cout << "应接近: (0, 1, 0)" << std::endl;

    // 测试组合旋转
    glm::quat rotX = glm::angleAxis(glm::radians(30.0f), glm::vec3(1, 0, 0));
    glm::quat rotY = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));
    transform.setRotation(rotY * rotX); // 先X后Y

    glm::mat3 combinedRot = glm::mat3_cast(transform.rotation);
    printMatrix3x3("\n组合旋转矩阵(X30°然后Y45°)", combinedRot);
}

// 测试10：正交投影视锥体测试
// ==================== 修复后的正交投影视锥体测试 ====================
void testOrthographicFrustum()
{
    std::cout << "\n=== 正交投影视锥体测试 ===" << std::endl;

    OrthographicCamera camera;
    camera.setScreenSize(800.0f, 600.0f);

    // 测试视锥体四个角 - 使用Vulkan坐标系
    glm::vec3 corners[] = {
        glm::vec3(camera.left, camera.top, 0.0f),     // 左上（Vulkan：Y=0是顶部）
        glm::vec3(camera.right, camera.top, 0.0f),    // 右上
        glm::vec3(camera.right, camera.bottom, 0.0f), // 右下
        glm::vec3(camera.left, camera.bottom, 0.0f)   // 左下
    };

    std::string cornerNames[] = {"左上", "右上", "右下", "左下"};

    // Vulkan NDC期望值
    glm::vec2 expectedNDC[] = {
        glm::vec2(-1.0f, -1.0f), // 左上：(-1, -1)
        glm::vec2(1.0f, -1.0f),  // 右上：(1, -1)
        glm::vec2(1.0f, 1.0f),   // 右下：(1, 1)   // Y向下！
        glm::vec2(-1.0f, 1.0f)   // 左下：(-1, 1)  // Y向下！
    };

    for (int i = 0; i < 4; i++)
    {
        glm::vec4 clipPos =
            camera.getViewProjectionMatrix() * glm::vec4(corners[i], 1.0f);
        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

        std::cout << cornerNames[i] << "角点: ";
        std::cout << "像素(" << corners[i].x << ", " << corners[i].y << ") → ";
        std::cout << "NDC(" << ndc.x << ", " << ndc.y << ")" << std::endl;

        // 检查是否接近期望值
        float tolerance = 0.01f;
        bool xOk = std::abs(ndc.x - expectedNDC[i].x) < tolerance;
        bool yOk = std::abs(ndc.y - expectedNDC[i].y) < tolerance;

        if (xOk && yOk)
        {
            std::cout << "  ✓ 正确映射到NDC(" << expectedNDC[i].x << ", "
                      << expectedNDC[i].y << ")" << std::endl;
        }
        else
        {
            std::cout << "  ✗ 映射错误，期望(" << expectedNDC[i].x << ", "
                      << expectedNDC[i].y << ")" << std::endl;
        }
    }
}
void validateCoordinateSystem()
{
    std::cout << "\n=== OrthographicCamera Vulkan标准验证 ===" << std::endl;

    // ==================== Vulkan NDC 标准定义 ====================
    std::cout << "\n[Vulkan NDC 坐标系标准]" << std::endl;
    std::cout << "根据Vulkan规范：" << std::endl;
    std::cout << "1. NDC坐标系（规范化设备坐标系）：" << std::endl;
    std::cout << "   X轴：左(-1.0) → 右(+1.0)" << std::endl;
    std::cout << "   Y轴：上(-1.0) → 下(+1.0)  // Y轴向下！" << std::endl;
    std::cout << "   Z轴：近(0.0) → 远(1.0)    // 与OpenGL [-1,1] 不同！" << std::endl;

    std::cout << "\n2. 屏幕空间到NDC的映射：" << std::endl;
    std::cout << "   屏幕左上角(0,0)     → NDC(-1,-1)" << std::endl;
    std::cout << "   屏幕中心(w/2,h/2)   → NDC(0,0)" << std::endl;
    std::cout << "   屏幕右下角(w-1,h-1) → NDC(+1,+1)" << std::endl;

    std::cout << "\n3. 正交投影矩阵要求：" << std::endl;
    std::cout << "   必须将上述映射正确实现" << std::endl;
    std::cout << "   矩阵公式：" << std::endl;
    std::cout << "   [ 2/(r-l),     0,         0,       -(r+l)/(r-l) ]" << std::endl;
    std::cout << "   [    0,    -2/(b-t),       0,       -(b+t)/(b-t) ]" << std::endl;
    std::cout << "   [    0,         0,     1/(f-n),        -n/(f-n)  ]" << std::endl;
    std::cout << "   [    0,         0,         0,             1      ]" << std::endl;

    // ==================== 创建测试相机 ====================
    std::cout << "\n\n[创建测试OrthographicCamera]" << std::endl;
    OrthographicCamera camera;
    camera.setScreenSize(800.0f, 600.0f);

    std::cout << "相机参数：" << std::endl;
    std::cout << "  left=" << camera.left << ", right=" << camera.right << std::endl;
    std::cout << "  bottom=" << camera.bottom << ", top=" << camera.top << std::endl;
    std::cout << "  near=" << camera.near << ", far=" << camera.far << std::endl;
    std::cout << "  注意：bottom(" << camera.bottom << ") > top(" << camera.top << ")"
              << std::endl;
    std::cout << "  这表示：Y轴向下（Vulkan风格）" << std::endl;

    // ==================== 测试1：投影矩阵验证 ====================
    std::cout << "\n\n[测试1：投影矩阵验证]" << std::endl;

    glm::mat4 proj = camera.getProjectionMatrix();
    printMatrix("相机生成的投影矩阵", proj);

    // 检查关键矩阵元素
    bool projPass = true;
    std::vector<std::string> projIssues;

    // 检查Y缩放因子（应为负，表示Y向下）
    if (proj[1][1] >= 0)
    {
        projPass = false;
        projIssues.push_back("M[1][1]应为负数（Y轴向下）");
    }

// 检查Z缩放因子（GLM_FORCE_DEPTH_ZERO_TO_ONE已定义，应为正）
#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
    if (proj[2][2] <= 0)
    {
        projPass = false;
        projIssues.push_back("M[2][2]应为正数（Z范围[0,1]）");
    }
#endif

    // 检查平移项
    float expectedM30 = -(camera.right + camera.left) / (camera.right - camera.left);
    float expectedM31 = -(camera.bottom + camera.top) / (camera.bottom - camera.top);

    if (std::abs(proj[3][0] - expectedM30) > 0.001f)
    {
        projPass = false;
        projIssues.push_back("M[3][0]值不正确");
    }

    if (std::abs(proj[3][1] - expectedM31) > 0.001f)
    {
        projPass = false;
        projIssues.push_back("M[3][1]值不正确");
    }

    if (projPass)
    {
        std::cout << "✓ 投影矩阵符合Vulkan标准" << std::endl;
    }
    else
    {
        std::cout << "✗ 投影矩阵有问题：" << std::endl;
        for (const auto &issue : projIssues)
        {
            std::cout << "  - " << issue << std::endl;
        }
    }

    // ==================== 测试2：关键点映射验证 ====================
    std::cout << "\n\n[测试2：关键点映射验证]" << std::endl;

    struct TestCase
    {
        glm::vec2 pixel;
        std::string description;
        glm::vec2 expectedNDC;
    };

    std::vector<TestCase> testCases = {
        {glm::vec2(0.0f, 0.0f), "左上角", glm::vec2(-1.0f, -1.0f)},
        {glm::vec2(400.0f, 300.0f), "中心", glm::vec2(0.0f, 0.0f)},
        {glm::vec2(800.0f, 600.0f), "右下角", glm::vec2(1.0f, 1.0f)},
        {glm::vec2(0.0f, 600.0f), "左下角", glm::vec2(-1.0f, 1.0f)},
        {glm::vec2(800.0f, 0.0f), "右上角", glm::vec2(1.0f, -1.0f)}};

    bool mappingPass = true;

    for (const auto &test : testCases)
    {
        auto result = camera.transformPixel(test.pixel.x, test.pixel.y);

        std::cout << "\n"
                  << test.description << " (" << test.pixel.x << "," << test.pixel.y
                  << "):" << std::endl;
        std::cout << "  计算NDC: (" << result.ndc.x << ", " << result.ndc.y << ")"
                  << std::endl;
        std::cout << "  期望NDC: (" << test.expectedNDC.x << ", " << test.expectedNDC.y
                  << ")" << std::endl;

        float error = glm::distance(result.ndc, test.expectedNDC);
        if (error < 0.01f)
        {
            std::cout << "  ✓ 映射正确" << std::endl;
        }
        else
        {
            std::cout << "  ✗ 映射错误，误差: " << error << std::endl;
            mappingPass = false;
        }
    }

    // ==================== 测试3：往返变换验证 ====================
    std::cout << "\n\n[测试3：往返变换验证]" << std::endl;

    bool roundtripPass = true;

    // 测试几个随机点
    std::vector<glm::vec2> testPoints = {
        glm::vec2(100.0f, 100.0f), glm::vec2(300.0f, 200.0f), glm::vec2(700.0f, 500.0f),
        glm::vec2(50.0f, 550.0f)};

    for (size_t i = 0; i < testPoints.size(); ++i)
    {
        const auto &pixel = testPoints[i];

        // 像素 → 世界
        glm::vec2 world = camera.pixelToWorld(pixel.x, pixel.y);

        // 世界 → 像素
        glm::vec2 pixel2 = camera.worldToPixel(world);

        float error = glm::distance(pixel, pixel2);

        std::cout << "\n测试点 " << (i + 1) << " (" << pixel.x << "," << pixel.y
                  << "):" << std::endl;
        std::cout << "  像素 → 世界: (" << world.x << "," << world.y << ")" << std::endl;
        std::cout << "  世界 → 像素: (" << pixel2.x << "," << pixel2.y << ")"
                  << std::endl;
        std::cout << "  往返误差: " << error << std::endl;

        if (error > 1.0f)
        { // 允许1像素的误差
            std::cout << "  ✗ 往返变换误差过大" << std::endl;
            roundtripPass = false;
        }
        else
        {
            std::cout << "  ✓ 往返变换一致" << std::endl;
        }
    }

    // ==================== 测试4：视图矩阵验证 ====================
    std::cout << "\n\n[测试4：视图矩阵验证]" << std::endl;

    // 测试相机变换
    camera.setPosition(glm::vec2(100.0f, 50.0f));
    camera.setRotation(glm::radians(30.0f));
    camera.zoom(2.0f);

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 viewProj = camera.getViewProjectionMatrix();

    std::cout << "相机变换后：" << std::endl;
    std::cout << "  位置: (" << camera.position.x << ", " << camera.position.y << ")"
              << std::endl;
    std::cout << "  旋转: " << glm::degrees(camera.rotation) << "度" << std::endl;
    std::cout << "  缩放: (" << camera.scale.x << ", " << camera.scale.y << ")"
              << std::endl;

    printMatrix("\n视图矩阵", view);
    printMatrix("\n视图投影矩阵", viewProj);

    // 测试视图矩阵的正确性
    // 将世界原点转换到视图空间
    glm::vec4 worldOrigin(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 viewOrigin = view * worldOrigin;

    std::cout << "\n视图空间原点测试：" << std::endl;
    std::cout << "  世界原点(0,0,0)在视图空间中: (" << viewOrigin.x << ", "
              << viewOrigin.y << ", " << viewOrigin.z << ")" << std::endl;

    // 由于相机位置是(100,50)，原点在视图空间中应该是(-100,-50)
    if (std::abs(viewOrigin.x + 100.0f) < 1.0f && std::abs(viewOrigin.y + 50.0f) < 1.0f)
    {
        std::cout << "  ✓ 视图矩阵正确（考虑了相机位置）" << std::endl;
    }
    else
    {
        std::cout << "  ✗ 视图矩阵可能有问题" << std::endl;
    }

    // ==================== 测试5：GLM宏配置验证 ====================
    std::cout << "\n\n[测试5：GLM宏配置验证]" << std::endl;

    std::cout << "检查GLM配置是否支持Vulkan：" << std::endl;

#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
    std::cout << "  ✓ GLM_FORCE_DEPTH_ZERO_TO_ONE: 支持Vulkan Z范围[0,1]" << std::endl;
#else
    std::cout << "  ✗ 未定义GLM_FORCE_DEPTH_ZERO_TO_ONE" << std::endl;
    std::cout << "    建议添加：#define GLM_FORCE_DEPTH_ZERO_TO_ONE" << std::endl;
#endif

#ifdef GLM_FORCE_RADIANS
    std::cout << "  ✓ GLM_FORCE_RADIANS: 使用弧度制" << std::endl;
#else
    std::cout << "  ⚠️ 未定义GLM_FORCE_RADIANS" << std::endl;
#endif

#ifdef GLM_FORCE_LEFT_HANDED
    std::cout << "  ⚠️ GLM_FORCE_LEFT_HANDED: 使用左手坐标系" << std::endl;
    std::cout << "     注意：Vulkan通常使用右手坐标系" << std::endl;
#else
    std::cout << "  ✓ 未定义GLM_FORCE_LEFT_HANDED: 使用右手坐标系" << std::endl;
#endif

    // ==================== 最终总结 ====================
    std::cout << "\n\n[最终验证结果]" << std::endl;

    bool allTestsPass = projPass && mappingPass && roundtripPass;

    if (allTestsPass)
    {
        std::cout << "🎉 恭喜！OrthographicCamera完全符合Vulkan标准" << std::endl;
        std::cout << "\n验证通过的项目：" << std::endl;
        std::cout << "  ✓ 投影矩阵正确" << std::endl;
        std::cout << "  ✓ 关键点映射正确" << std::endl;
        std::cout << "  ✓ 往返变换一致" << std::endl;
        std::cout << "  ✓ 视图矩阵正确" << std::endl;
        std::cout << "  ✓ GLM配置支持Vulkan" << std::endl;
    }
    else
    {
        std::cout << "⚠️ OrthographicCamera需要修复：" << std::endl;
        if (!projPass)
            std::cout << "  - 投影矩阵有问题" << std::endl;
        if (!mappingPass)
            std::cout << "  - 点映射不正确" << std::endl;
        if (!roundtripPass)
            std::cout << "  - 往返变换不一致" << std::endl;

        std::cout << "\n修复建议：" << std::endl;
        std::cout << "1. 检查getProjectionMatrix()实现" << std::endl;
        std::cout << "2. 检查transformPixel()和pixelToWorld()公式" << std::endl;
        std::cout << "3. 确保使用正确的Vulkan NDC标准" << std::endl;
    }

    std::cout << "\n[验证完成]" << std::endl;
}
// ==================== 坐标系方向测试 ====================

// 添加在OrthographicCamera类定义之后，测试函数之前

class CoordinateSystemTests
{
  public:
    // 测试1：坐标系类型判断（右手系 vs 左手系）
    static void testCoordinateSystemType()
    {
        std::cout << "\n=== 坐标系类型测试 ===" << std::endl;

        // 测试投影矩阵的坐标系
        OrthographicCamera camera;
        camera.setScreenSize(800, 600);

        glm::mat4 proj = camera.getProjectionMatrix();

        // 检查投影矩阵的特征
        // 对于正交投影：
        // - 右手系：top < bottom (Y轴向上)
        // - 左手系：top > bottom (Y轴向下)
        if (camera.top < camera.bottom)
        {
            std::cout << "投影矩阵使用: 右手坐标系 (Y轴向上)" << std::endl;
            std::cout << "  top(" << camera.top << ") < bottom(" << camera.bottom << ")"
                      << std::endl;
        }
        else
        {
            std::cout << "投影矩阵使用: 左手坐标系 (Y轴向下)" << std::endl;
            std::cout << "  top(" << camera.top << ") > bottom(" << camera.bottom << ")"
                      << std::endl;
        }

        // 检查NDC坐标系的Y轴方向
        auto result = camera.transformPixel(0, 0); // 左上角
        if (result.ndc.y > 0)
        {
            std::cout << "NDC坐标系: Y轴向上 (左上角→正Y)" << std::endl;
        }
        else
        {
            std::cout << "NDC坐标系: Y轴向下 (左上角→负Y)" << std::endl;
        }
    }

    // 测试2：旋转方向判断（顺时针 vs 逆时针）
    static void testRotationDirection()
    {
        std::cout << "\n=== 旋转方向测试 ===" << std::endl;

        OrthographicCamera camera;
        camera.setScreenSize(800, 600);
        camera.setPosition(glm::vec2(400, 300)); // 中心

        // 测试正向旋转（按弧度定义）
        std::cout << "测试绕Z轴旋转方向：" << std::endl;

        // 创建测试点：在相机右侧
        glm::vec2 testPoint(600, 300); // 右侧中心点

        // 记录原始位置
        glm::vec2 originalPixel = camera.worldToPixel(testPoint);

        // 顺时针旋转45度
        camera.setRotation(glm::radians(-45.0f));
        glm::vec2 clockwisePixel = camera.worldToPixel(testPoint);

        // 逆时针旋转45度
        camera.setRotation(glm::radians(45.0f));
        glm::vec2 counterClockwisePixel = camera.worldToPixel(testPoint);

        std::cout << "  原始位置: (" << originalPixel.x << ", " << originalPixel.y << ")"
                  << std::endl;
        std::cout << "  顺时针(-45°): (" << clockwisePixel.x << ", " << clockwisePixel.y
                  << ")" << std::endl;
        std::cout << "  逆时针(+45°): (" << counterClockwisePixel.x << ", "
                  << counterClockwisePixel.y << ")" << std::endl;

        // 判断旋转方向
        if (clockwisePixel.y < originalPixel.y &&
            counterClockwisePixel.y > originalPixel.y)
        {
            std::cout << "  旋转方向: +角度=逆时针，-角度=顺时针 (标准数学方向)"
                      << std::endl;
        }
        else if (clockwisePixel.y > originalPixel.y &&
                 counterClockwisePixel.y < originalPixel.y)
        {
            std::cout << "  旋转方向: +角度=顺时针，-角度=逆时针 (反转方向)" << std::endl;
        }
    }

    // 测试3：坐标轴方向判断
    static void testAxisDirections()
    {
        std::cout << "\n=== 坐标轴方向测试 ===" << std::endl;

        OrthographicCamera camera;
        camera.setScreenSize(800, 600);

        // 测试X轴方向
        std::cout << "X轴方向测试：" << std::endl;
        auto left = camera.transformPixel(0, 300);
        auto right = camera.transformPixel(800, 300);

        std::cout << "  屏幕左边界X=" << left.ndc.x << " (应为-1)" << std::endl;
        std::cout << "  屏幕右边界X=" << right.ndc.x << " (应为+1)" << std::endl;

        if (left.ndc.x < right.ndc.x)
        {
            std::cout << "  X轴方向: 左(-) → 右(+) (标准方向)" << std::endl;
        }
        else
        {
            std::cout << "  X轴方向: 右(-) → 左(+) (反转方向)" << std::endl;
        }

        // 测试Y轴方向
        std::cout << "Y轴方向测试：" << std::endl;
        auto top = camera.transformPixel(400, 0);
        auto bottom = camera.transformPixel(400, 600);

        std::cout << "  屏幕上边界Y=" << top.ndc.y << std::endl;
        std::cout << "  屏幕下边界Y=" << bottom.ndc.y << std::endl;

        if (top.ndc.y < bottom.ndc.y)
        {
            std::cout << "  Y轴方向: 上(-) → 下(+) (Y轴向下，Vulkan风格)" << std::endl;
        }
        else
        {
            std::cout << "  Y轴方向: 下(-) → 上(+) (Y轴向上，OpenGL风格)" << std::endl;
        }
    }

    // 测试4：四元数旋转方向验证
    static void testQuaternionRotationDirection()
    {
        std::cout << "\n=== 四元数旋转方向测试 ===" << std::endl;

        Transform transform;

        // 测试绕不同轴的旋转
        std::vector<std::tuple<std::string, glm::vec3, glm::vec3, glm::vec3>> tests = {
            {"绕X轴+90度", glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)},
            {"绕Y轴+90度", glm::vec3(0, 1, 0), glm::vec3(0, 0, -1), glm::vec3(-1, 0, 0)},
            {"绕Z轴+90度", glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)}};

        for (const auto &[name, axis, input, expectedRightHand] : tests)
        {
            glm::quat rotation = glm::angleAxis(glm::radians(90.0f), axis);
            glm::vec3 result = rotation * input;

            std::cout << name << ":" << std::endl;
            std::cout << "  输入: (" << input.x << ", " << input.y << ", " << input.z
                      << ")" << std::endl;
            std::cout << "  结果: (" << result.x << ", " << result.y << ", " << result.z
                      << ")" << std::endl;
            std::cout << "  右手系期望: (" << expectedRightHand.x << ", "
                      << expectedRightHand.y << ", " << expectedRightHand.z << ")"
                      << std::endl;

            // 判断是右手系还是左手系结果
            glm::vec3 expectedLeftHand = -expectedRightHand; // 左手系是相反方向

            float rhError = glm::distance(result, expectedRightHand);
            float lhError = glm::distance(result, expectedLeftHand);

            if (rhError < lhError)
            {
                std::cout << "  → 使用右手坐标系旋转" << std::endl;
            }
            else
            {
                std::cout << "  → 使用左手坐标系旋转" << std::endl;
            }
        }
    }

    // 测试5：相机移动方向测试
    static void testCameraMovementDirections()
    {
        std::cout << "\n=== 相机移动方向测试 ===" << std::endl;

        // 测试2D相机
        OrthographicCamera camera2D;
        camera2D.setScreenSize(800, 600);

        // 记录原始位置
        glm::vec2 originalPos = camera2D.position;
        glm::vec2 testPoint(100, 100);
        glm::vec2 originalPixel = camera2D.worldToPixel(testPoint);

        // 向右移动相机
        camera2D.translate(glm::vec2(50, 0));
        glm::vec2 rightPixel = camera2D.worldToPixel(testPoint);

        // 恢复并向下移动
        camera2D.setPosition(originalPos);
        camera2D.translate(glm::vec2(0, 50));
        glm::vec2 downPixel = camera2D.worldToPixel(testPoint);

        std::cout << "相机向右移动50像素：" << std::endl;
        std::cout << "  物体屏幕X坐标: " << originalPixel.x << " → " << rightPixel.x
                  << std::endl;
        std::cout << "  物体屏幕Y坐标: " << originalPixel.y << " → " << rightPixel.y
                  << std::endl;

        if (rightPixel.x < originalPixel.x)
        {
            std::cout << "  → 相机右移，物体向左移动 (正确)" << std::endl;
        }
        else
        {
            std::cout << "  → 相机右移，物体向右移动 (错误！)" << std::endl;
        }

        std::cout << "相机向下移动50像素：" << std::endl;
        std::cout << "  物体屏幕X坐标: " << originalPixel.x << " → " << downPixel.x
                  << std::endl;
        std::cout << "  物体屏幕Y坐标: " << originalPixel.y << " → " << downPixel.y
                  << std::endl;

        if (downPixel.y < originalPixel.y)
        {
            std::cout << "  → 相机下移，物体向上移动 (正确)" << std::endl;
        }
        else
        {
            std::cout << "  → 相机下移，物体向下移动 (错误！)" << std::endl;
        }
    }

    // 测试6：投影矩阵缩放方向测试
    static void testProjectionScaling()
    {
        std::cout << "\n=== 投影矩阵缩放测试 ===" << std::endl;

        OrthographicCamera camera;
        camera.setScreenSize(800, 600);

        // 测试点：屏幕中心
        glm::vec2 center(400, 300);
        auto original = camera.transformPixel(center.x, center.y);

        // 缩放相机
        camera.zoom(2.0f); // 放大2倍
        auto zoomed = camera.transformPixel(center.x, center.y);

        std::cout << "相机放大2倍：" << std::endl;
        std::cout << "  原始NDC: (" << original.ndc.x << ", " << original.ndc.y << ")"
                  << std::endl;
        std::cout << "  放大后NDC: (" << zoomed.ndc.x << ", " << zoomed.ndc.y << ")"
                  << std::endl;

        if (glm::length(zoomed.ndc) < glm::length(original.ndc))
        {
            std::cout << "  → 放大后NDC坐标更接近中心 (正确)" << std::endl;
        }
        else
        {
            std::cout << "  → 放大后NDC坐标更远离中心 (错误！)" << std::endl;
        }
    }

    // 测试7：3D相机方向测试
    static void test3DCameraDirections()
    {
        std::cout << "\n=== 3D相机方向测试 ===" << std::endl;

        QuaternionCamera camera3D;

        // 测试前向向量
        glm::vec3 forward = camera3D.getForward();
        std::cout << "相机前向向量: (" << forward.x << ", " << forward.y << ", "
                  << forward.z << ")" << std::endl;

        // 测试向上向量
        glm::vec3 up = camera3D.getUp();
        std::cout << "相机向上向量: (" << up.x << ", " << up.y << ", " << up.z << ")"
                  << std::endl;

        // 测试右向向量
        glm::vec3 right = camera3D.getRight();
        std::cout << "相机右向向量: (" << right.x << ", " << right.y << ", " << right.z
                  << ")" << std::endl;

        // 验证坐标系手性
        float dot = glm::dot(glm::cross(forward, up), right);
        if (dot > 0)
        {
            std::cout << "  → 使用右手坐标系 (forward × up = right)" << std::endl;
        }
        else
        {
            std::cout << "  → 使用左手坐标系 (forward × up = -right)" << std::endl;
        }

        // 测试旋转方向
        std::cout << "\n测试绕Y轴旋转：" << std::endl;
        glm::vec3 originalForward = forward;

        // 正角度旋转
        camera3D.rotateYaw(90.0f);
        glm::vec3 rotatedForward = camera3D.getForward();

        std::cout << "  原始前向: (" << originalForward.x << ", " << originalForward.y
                  << ", " << originalForward.z << ")" << std::endl;
        std::cout << "  旋转+90°后: (" << rotatedForward.x << ", " << rotatedForward.y
                  << ", " << rotatedForward.z << ")" << std::endl;

        // 根据叉积判断旋转方向
        glm::vec3 cross = glm::cross(originalForward, rotatedForward);
        if (cross.y > 0)
        {
            std::cout << "  → +角度绕Y轴逆时针旋转 (右手系)" << std::endl;
        }
        else if (cross.y < 0)
        {
            std::cout << "  → +角度绕Y轴顺时针旋转 (左手系)" << std::endl;
        }
    }

    // 运行所有测试
    static void runAllTests()
    {
        std::cout << "开始坐标系方向测试..." << std::endl;

        testCoordinateSystemType();
        testAxisDirections();
        testRotationDirection();
        testQuaternionRotationDirection();
        testCameraMovementDirections();
        testProjectionScaling();
        test3DCameraDirections();

        std::cout << "\n所有坐标系方向测试完成!" << std::endl;
    }
};
// 运行所有静态测试
void runAllStaticTests()
{
    std::cout << "开始运行静态MVP测试...\n" << std::endl;

    // 首先运行坐标系测试
    validateCoordinateSystem();
    // CoordinateSystemTests::runAllTests();

    // testPerspectiveMVP();
    // testOrthographicMVP();
    // testQuaternionRotation();
    // testDepthRange();
    // testMatrixMultiplicationOrder();
    // test2DCameraTransform();
    // testCameraTransformationChain();
    // testMVPInverse();
    // testQuaternionDirectionConsistency();
    // testOrthographicFrustum();

    std::cout << "\n所有静态测试完成!" << std::endl;
}

// ==================== 创建2D UI几何体的辅助函数 ====================
std::vector<Vertex> createUIRectangle(float x, float y, float width, float height,
                                      const glm::vec3 &color)
{
    // 注意：2D UI使用像素坐标，Y轴向下
    // 创建矩形的4个顶点（顺时针顺序）
    return {
        {{x, y, 0.0f}, color},                  // 左上
        {{x + width, y, 0.0f}, color},          // 右上
        {{x + width, y + height, 0.0f}, color}, // 右下
        {{x, y + height, 0.0f}, color}          // 左下
    };
}

std::vector<Vertex> createUICircle(float centerX, float centerY, float radius,
                                   int segments, const glm::vec3 &color)
{
    std::vector<Vertex> vertices;
    vertices.reserve(segments);

    for (int i = 0; i < segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        vertices.push_back({{x, y, 0.0f}, color});
    }

    return vertices;
}

std::vector<uint32_t> createUIIndices(int vertexCount)
{
    std::vector<uint32_t> indices;
    indices.reserve((vertexCount - 2) * 3);

    // 三角形扇索引
    for (int i = 1; i < vertexCount - 1; ++i)
    {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    return indices;
}

// ==================== 工具函数 ====================
template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

// ==================== 动态MVP测试标志 ====================
struct DynamicTestFlags
{
    bool test3DAnimation = false;    // 3D动态动画测试
    bool test2DAnimation = false;    // 2D UI动态动画测试
    bool testCameraOrbit = false;    // 相机轨道运动测试
    bool testUIMovement = false;     // UI移动测试
    bool testDepthAnimation = false; // 深度动画测试

    void reset()
    {
        *this = DynamicTestFlags();
    }
};

// 在全局作用域添加这些函数
VkFormat findDepthFormat(physical_device &physicalDevice)
{
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};

    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice.raw_data(), format, &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported depth format!");
}

VkFormat findSupportedFormat(physical_device &physicalDevice,
                             const std::vector<VkFormat> &candidates,
                             VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice.raw_data(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                 (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

int main()
{
    try
    {
        // ==================== 运行静态测试 ====================
        runAllStaticTests();

        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
            enablefeatureChain = {
                {},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE}};

        surface window{};
        window.setup({.width = WIDTH, .height = HEIGHT}, TITLE);

        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "2D/3D MVP测试框架",
                                    .applicationVersion = VkApiVersion(1, 0, 0),
                                    .pEngineName = "No Engine",
                                    .engineVersion = VkApiVersion(1, 0, 0),
                                    .apiVersion = VkApiVersion(0, 1, 3, 0)});

        VkSurfaceKHR surface_ = window.createVkSurfaceKHR(instance.ref_data());
        assert(surface_ != nullptr);

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
                        auto query = structure_chain<
                            VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{{}, {}, {}};
                        physicalDevice.getFeatures2(query.head());
                        auto &query_vulkan13_features =
                            query.template get<VkPhysicalDeviceVulkan13Features>();
                        auto &query_extended_dynamic_state_features = query.template get<
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                        return query_vulkan13_features.dynamicRendering &&
                               query_vulkan13_features.synchronization2 &&
                               query_extended_dynamic_state_features.extendedDynamicState;
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
        auto *presentQueue = logical_device_.getDeviceQueue(graphicsIndex, 0);

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
            minImageCount = (surfaceCapabilities.maxImageCount > 0 &&
                             minImageCount > surfaceCapabilities.maxImageCount)
                                ? surfaceCapabilities.maxImageCount
                                : minImageCount;

            VkSwapchainCreateInfoKHR swapChainCreateInfo{
                .sType = sType<VkSwapchainCreateInfoKHR>(),
                .surface = surface_,
                .minImageCount = minImageCount,
                .imageFormat = swapChainImageFormat,
                .imageColorSpace = swapChainFormat.colorSpace,
                .imageExtent = swapChainExtent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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

        // 在createSwapChain之后添加这些变量
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkImage depthImage = VK_NULL_HANDLE;
        VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;

        // 创建深度资源的函数
        auto createDepthResources = [&]() {
            // 查找深度格式
            depthFormat = findDepthFormat(physicalDevice);

            // 创建深度图像
            VkImageCreateInfo imageInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
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

            // 分配内存
            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(logical_device_.raw_data(), depthImage,
                                         &memRequirements);

            VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = physicalDevice.findMemoryType(
                    memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

            depthImageMemory = logical_device_.allocateMemory(allocInfo);
            vkBindImageMemory(logical_device_.raw_data(), depthImage, depthImageMemory,
                              0);

            // 创建深度图像视图
            VkImageViewCreateInfo viewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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
        createDepthResources();

        // 清理深度资源的函数
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

        // ==================== 创建场景 ====================
        Scene scene;

        // ==================== 创建3D几何体 ====================
        std::vector<Vertex> baseVertices3D = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // 左下，红色
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // 右下，绿色
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},   // 右上，蓝色
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}}   // 左上，黄色
        };
        std::vector<uint32_t> baseIndices3D = {0, 1, 2, 2, 3, 0};

        // 将2D平面改为3D立方体，以更好地测试深度
        std::vector<Vertex> cube3D = {// 前面（红色）
                                      {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},

                                      // 后面（绿色）
                                      {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}};

        std::vector<uint32_t> cubeInx = {// 前面
                                         0, 1, 2, 2, 3, 0,
                                         // 后面
                                         4, 5, 6, 6, 7, 4,
                                         // 上面
                                         3, 2, 6, 6, 7, 3,
                                         // 下面
                                         0, 1, 5, 5, 4, 0,
                                         // 左面
                                         0, 3, 7, 7, 4, 0,
                                         // 右面
                                         1, 2, 6, 6, 5, 1};

        RenderObject obj3D_0(cube3D, cubeInx, false);
        obj3D_0.transform.setPosition(glm::vec3(-1.0f, 0.0f, 0.0f));
        scene.addObject3D(std::move(obj3D_0));

        // 创建3D物体
        RenderObject obj3D_1(baseVertices3D, baseIndices3D, false);
        obj3D_1.transform.setPosition(glm::vec3(-0.0f, 0.0f, 0.0f));
        scene.addObject3D(std::move(obj3D_1));

        RenderObject obj3D_2(baseVertices3D, baseIndices3D, false);
        obj3D_2.transform.setPosition(glm::vec3(1.0f, 0.0f, 0.0f));
        obj3D_2.transform.setScale(glm::vec3(0.5f, 0.5f, 0.5f));
        scene.addObject3D(std::move(obj3D_2));

        // ==================== 创建2D UI几何体 ====================
        // UI矩形1：红色状态栏
        auto uiRect1Verts =
            createUIRectangle(50.0f, 50.0f, 200.0f, 40.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        std::vector<uint32_t> uiRect1Indices = {0, 1, 2, 2, 3, 0};
        RenderObject obj2D_1(uiRect1Verts, uiRect1Indices, true);
        scene.addObject2D(std::move(obj2D_1));

        // UI矩形2：绿色按钮
        auto uiRect2Verts =
            createUIRectangle(300.0f, 100.0f, 100.0f, 50.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        RenderObject obj2D_2(uiRect2Verts, uiRect1Indices, true);
        scene.addObject2D(std::move(obj2D_2));

        // UI圆形：蓝色图标
        auto uiCircleVerts =
            createUICircle(600.0f, 100.0f, 30.0f, 32, glm::vec3(0.0f, 0.0f, 1.0f));
        auto uiCircleIndices = createUIIndices(static_cast<int>(uiCircleVerts.size()));
        RenderObject obj2D_3(uiCircleVerts, uiCircleIndices, true);
        scene.addObject2D(std::move(obj2D_3));

        // 为所有渲染对象创建MeshData
        std::vector<std::unique_ptr<MeshData>> meshDatas3D;
        std::vector<std::unique_ptr<MeshData>> meshDatas2D;

        for (const auto &obj : scene.getObjects3D())
        {
            meshDatas3D.push_back(std::make_unique<MeshData>(
                physicalDevice, logical_device_, graphicsQueue, commandPool, obj.vertices,
                obj.indices, false));
        }

        for (const auto &obj : scene.getObjects2D())
        {
            meshDatas2D.push_back(
                std::make_unique<MeshData>(physicalDevice, logical_device_, graphicsQueue,
                                           commandPool, obj.vertices, obj.indices, true));
        }

        // ==================== 创建3D图形管线 ====================
        auto createGraphicsPipeline3D = [&]() -> std::pair<VkPipelineLayout, VkPipeline> {
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

            // 3D管线：启用深度测试
            VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            // 深度测试配置
            VkPipelineDepthStencilStateCreateInfo depthStencil = {
                .sType = sType<VkPipelineDepthStencilStateCreateInfo>(),
                .depthTestEnable = VK_TRUE,           // 启用深度测试
                .depthWriteEnable = VK_TRUE,          // 启用深度写入
                .depthCompareOp = VK_COMPARE_OP_LESS, // 深度比较：小于
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable = VK_FALSE};

            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE,
            };

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

            VkDescriptorSetLayout descriptorSetLayout =
                meshDatas3D[0]->getDescriptorSetLayout();
            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(),
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout};

            VkPipelineLayout pipelineLayout = nullptr;
            VkPipeline graphicsPipeline = nullptr;

            try
            {
                pipelineLayout =
                    logical_device_.createPipelineLayout(pipelineLayoutInfo, nullptr);
                MeshData::pipelineLayout = pipelineLayout;

                // 在createGraphicsPipeline3D函数中，修改VkPipelineRenderingCreateInfo
                structure_chain<VkGraphicsPipelineCreateInfo,
                                VkPipelineRenderingCreateInfo>
                    pipelineCreateInfoChain = {
                        {.stageCount = 2,
                         .pStages = shaderStages,
                         .pVertexInputState = &vertexInputInfo,
                         .pInputAssemblyState = &inputAssembly,
                         .pViewportState = &viewportState,
                         .pRasterizationState = &rasterizer,
                         .pDepthStencilState = &depthStencil,
                         .pMultisampleState = &multisampling,
                         .pColorBlendState = &colorBlending,
                         .pDynamicState = &dynamicState,
                         .layout = pipelineLayout,
                         .renderPass = VK_NULL_HANDLE},
                        {.colorAttachmentCount = 1,
                         .pColorAttachmentFormats = &swapChainImageFormat,
                         .depthAttachmentFormat = depthFormat} // 添加深度附件格式
                    };

                graphicsPipeline = logical_device_.createGraphicsPipelines(
                    nullptr, 1, pipelineCreateInfoChain.head(), nullptr);
                return std::make_pair(pipelineLayout, graphicsPipeline);
            }
            catch (...)
            {
                if (graphicsPipeline != nullptr)
                {
                    logical_device_.destroyPipeline(graphicsPipeline, nullptr);
                    graphicsPipeline = nullptr;
                }
                if (pipelineLayout != nullptr)
                {
                    logical_device_.destroyPipelineLayout(pipelineLayout, nullptr);
                    pipelineLayout = nullptr;
                }
                throw;
            }
        };

        // ==================== 创建2D UI图形管线 ====================
        auto createGraphicsPipeline2D = [&]() -> std::pair<VkPipelineLayout, VkPipeline> {
            mcs::vulkan::shader_module vertshader{logical_device_, UI_VERT_SHADER_PATH};
            mcs::vulkan::shader_module fragshader{logical_device_, UI_FRAG_SHADER_PATH};

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

            // 2D管线：禁用背面剔除和深度测试
            VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE, // 2D UI通常不需要背面剔除
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            // 2D UI：禁用深度测试
            VkPipelineDepthStencilStateCreateInfo depthStencil = {
                .sType = sType<VkPipelineDepthStencilStateCreateInfo>(),
                .depthTestEnable = VK_FALSE,  // 禁用深度测试
                .depthWriteEnable = VK_FALSE, // 禁用深度写入
                .depthCompareOp = VK_COMPARE_OP_ALWAYS,
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable = VK_FALSE};

            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE,
            };

            // 2D UI可能需要alpha混合
            VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .blendEnable = VK_TRUE, // 启用混合
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
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

            VkDescriptorSetLayout descriptorSetLayout =
                meshDatas2D[0]->getDescriptorSetLayout();
            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = sType<VkPipelineLayoutCreateInfo>(),
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout};

            VkPipelineLayout pipelineLayout = nullptr;
            VkPipeline graphicsPipeline = nullptr;

            try
            {
                pipelineLayout =
                    logical_device_.createPipelineLayout(pipelineLayoutInfo, nullptr);
                MeshData::pipelineLayout2D = pipelineLayout;

                // 修改2D管线的VkPipelineRenderingCreateInfo
                structure_chain<VkGraphicsPipelineCreateInfo,
                                VkPipelineRenderingCreateInfo>
                    pipelineCreateInfoChain = {
                        {.stageCount = 2,
                         .pStages = shaderStages,
                         .pVertexInputState = &vertexInputInfo,
                         .pInputAssemblyState = &inputAssembly,
                         .pViewportState = &viewportState,
                         .pRasterizationState = &rasterizer,
                         .pDepthStencilState = &depthStencil, // 2D禁用深度测试
                         .pMultisampleState = &multisampling,
                         .pColorBlendState = &colorBlending,
                         .pDynamicState = &dynamicState,
                         .layout = pipelineLayout,
                         .renderPass = VK_NULL_HANDLE},
                        {.colorAttachmentCount = 1,
                         .pColorAttachmentFormats = &swapChainImageFormat,
                         // 关键修复：明确设置深度附件格式为UNDEFINED
                         .depthAttachmentFormat = VK_FORMAT_UNDEFINED}};

                graphicsPipeline = logical_device_.createGraphicsPipelines(
                    nullptr, 1, pipelineCreateInfoChain.head(), nullptr);
                return std::make_pair(pipelineLayout, graphicsPipeline);
            }
            catch (...)
            {
                if (graphicsPipeline != nullptr)
                {
                    logical_device_.destroyPipeline(graphicsPipeline, nullptr);
                    graphicsPipeline = nullptr;
                }
                if (pipelineLayout != nullptr)
                {
                    logical_device_.destroyPipelineLayout(pipelineLayout, nullptr);
                    pipelineLayout = nullptr;
                }
                throw;
            }
        };

        // 创建两个管线
        auto [pipelineLayout3D, graphicsPipeline3D] = createGraphicsPipeline3D();
        auto [pipelineLayout2D, graphicsPipeline2D] = createGraphicsPipeline2D();

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
            cleanupDepthResources();

            createSwapChain();
            createImageViews();
            createDepthResources();

            // 更新2D相机尺寸（重要！）
            auto &camera2d = scene.getCamera2D();
            camera2d.right = static_cast<float>(swapChainExtent.width);
            camera2d.bottom = static_cast<float>(swapChainExtent.height);

            // 更新3D相机宽高比
            auto &camera3d = scene.getCamera3D();
            camera3d.updateAspectRatio(static_cast<float>(swapChainExtent.width),
                                       static_cast<float>(swapChainExtent.height));
        };
        // ==================== 动态MVP测试配置 ====================
        DynamicTestFlags dynamicTests;

        // 启用需要的动态测试
        // dynamicTests.test3DAnimation = true;    // 3D动画
        // dynamicTests.test2DAnimation = true;    // 2D UI动画
        // dynamicTests.testCameraOrbit = true;    // 相机轨道（可选）
        // dynamicTests.testUIMovement = true;     // UI移动
        // dynamicTests.testDepthAnimation = true; // 深度动画

        // 动画计时器
        static float animationTime = 0.0f;

        auto &camera2d = scene.getCamera2D();

        auto &camera3d = scene.getCamera3D();

        // NOTE: 相机静态操作. 2D 不会放大和缩小, 3D 会随着窗口变大变小
        //  旋转相机45度
        // camera2d.setRotation(glm::radians(45.0f));
        //  // 移动相机.相机往上移动,就是2D的画面往下平移
        // camera2d.translate(glm::vec2(0, 50));
        //  // 缩放
        //  camera2d.zoom(1.5f);

        camera3d.rotateRoll(45.0f);

        /*
1. 两个独立的渲染阶段：
第一阶段（3D）：使用深度附件，清除颜色和深度缓冲
第二阶段（2D）：不使用深度附件，加载现有的颜色缓冲（不清除）

2. 不同的加载操作：
// 第一阶段：清除颜色和深度
.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR
// 第二阶段：加载现有颜色内容
.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD

3. 深度附件处理：
// 第一阶段：使用深度附件
.pDepthAttachment = &depthAttachment3D
// 第二阶段：不使用深度附件
.pDepthAttachment = nullptr

4. 动画逻辑分离：
3D动画在第一阶段更新和渲染
2D UI动画在第二阶段更新和渲染

5. 性能优化点：
深度缓冲只在3D阶段使用和访问
2D阶段不绑定深度缓冲，减少内存带宽
颜色缓冲在第一阶段被清除，第二阶段重用

*/
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];

            if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // 转换交换链图像布局
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            // 转换深度图像布局（只用于3D渲染）
            my_render::transition_image_layout(
                commandBuffer, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.1F, 0.1F, 0.1F, 1.0F}}};
            VkClearValue depthClear = {.depthStencil = {.depth = 1.0f}};

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};

            // ==================== 动态MVP测试 ====================
            auto &camera3D = scene.getCamera3D();
            auto &camera2D = scene.getCamera2D();

            // 获取可修改的对象引用
            auto &objects3D =
                const_cast<std::vector<RenderObject> &>(scene.getObjects3D());
            auto &objects2D =
                const_cast<std::vector<RenderObject> &>(scene.getObjects2D());

            // ==================== 第一阶段：渲染3D场景（使用深度附件）
            {
                VkRenderingAttachmentInfo colorAttachment3D = {
                    .sType = sType<VkRenderingAttachmentInfo>(),
                    .imageView = imageView,
                    .imageLayout =
                        VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = clearColor};

                VkRenderingAttachmentInfo depthAttachment3D = {
                    .sType = sType<VkRenderingAttachmentInfo>(),
                    .imageView = depthImageView,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .clearValue = depthClear};

                VkRenderingInfo renderingInfo3D = {
                    .sType = sType<VkRenderingInfo>(),
                    .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &colorAttachment3D,
                    .pDepthAttachment = &depthAttachment3D, // 3D使用深度附件
                    .pStencilAttachment = nullptr};

                ::vkCmdBeginRendering(commandBuffer, &renderingInfo3D);

                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphicsPipeline3D);

                ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                // 更新动画时间
                animationTime += 0.016f; // 假设60fps

                // 动态测试A：3D物体动画
                if (dynamicTests.test3DAnimation && objects3D.size() >= 2)
                {
                    float time = animationTime;

                    // 第一个物体：旋转
                    auto &obj1 = objects3D[0];
                    obj1.transform.setRotation(
                        glm::angleAxis(time, glm::vec3(0.0f, 1.0f, 0.0f)));

                    // 第二个物体：上下浮动
                    auto &obj2 = objects3D[1];
                    float yOffset = sin(time * 2.0f) * 0.5f;
                    obj2.transform.setPosition(glm::vec3(1.0f, yOffset, 0.0f));
                }

                // 动态测试B：相机轨道运动
                if (dynamicTests.testCameraOrbit)
                {
                    float orbitRadius = 3.0f;
                    float orbitSpeed = 0.5f;
                    float orbitAngle = animationTime * orbitSpeed;

                    camera3D.position.x = sin(orbitAngle) * orbitRadius;
                    camera3D.position.z = cos(orbitAngle) * orbitRadius - 3.0f;
                    camera3D.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
                }

                // 动态测试D：深度动画
                if (dynamicTests.testDepthAnimation && objects3D.size() >= 1)
                {
                    auto &obj = objects3D[0];
                    float depth = sin(animationTime) * 2.0f - 1.0f;
                    obj.transform.setPosition(glm::vec3(-1.0f, 0.0f, depth));
                }

                // 渲染所有3D物体
                for (size_t i = 0; i < objects3D.size(); i++)
                {
                    if (i < meshDatas3D.size())
                    {
                        glm::mat4 mvp = objects3D[i].calculateMVP(camera3D);
                        meshDatas3D[i]->updateUniformBuffer(currentFrame, mvp);
                        meshDatas3D[i]->bind(commandBuffer, currentFrame);
                        meshDatas3D[i]->draw(commandBuffer);
                    }
                }

                ::vkCmdEndRendering(commandBuffer);
            }

            // ==================== 第二阶段：渲染2D UI（不使用深度附件）
            {
                // 注意：这里使用LOAD操作而不是CLEAR，以保留3D渲染的结果
                VkRenderingAttachmentInfo colorAttachment2D = {
                    .sType = sType<VkRenderingAttachmentInfo>(),
                    .imageView = imageView,
                    .imageLayout =
                        VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, // 加载现有内容（不清除）
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = clearColor};

                VkRenderingInfo renderingInfo2D = {
                    .sType = sType<VkRenderingInfo>(),
                    .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &colorAttachment2D,
                    .pDepthAttachment = nullptr, // 2D不使用深度附件
                    .pStencilAttachment = nullptr};

                ::vkCmdBeginRendering(commandBuffer, &renderingInfo2D);

                // 切换到2D管线
                ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    graphicsPipeline2D);

                // 动态测试C：2D UI动画
                if (dynamicTests.test2DAnimation && objects2D.size() >= 3)
                {
                    float time = animationTime;

                    // 第一个UI：脉动效果
                    auto &ui1 = objects2D[0];
                    float pulse = sin(time * 3.0f) * 0.2f + 1.0f;
                    ui1.transform.setScale(glm::vec3(1.0f, pulse, 1.0f));

                    // 第二个UI：左右移动
                    auto &ui2 = objects2D[1];
                    float moveX = sin(time * 2.0f) * 100.0f + 300.0f;
                    ui2.transform.setPosition(glm::vec3(moveX, 100.0f, 0.0f));

                    // 第三个UI：旋转
                    auto &ui3 = objects2D[2];
                    ui3.transform.setRotation(
                        glm::angleAxis(time, glm::vec3(0.0f, 0.0f, 1.0f)));
                }

                // 渲染所有2D UI物体
                for (size_t i = 0; i < objects2D.size(); i++)
                {
                    if (i < meshDatas2D.size())
                    {
                        glm::mat4 mvp = objects2D[i].calculateMVP(camera2D);
                        meshDatas2D[i]->updateUniformBuffer(currentFrame, mvp);
                        meshDatas2D[i]->bind(commandBuffer, currentFrame);
                        meshDatas2D[i]->draw(commandBuffer);
                    }
                }

                ::vkCmdEndRendering(commandBuffer);
            }

            // 转换深度图像布局回初始状态
            my_render::transition_image_layout(
                commandBuffer, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

            // 从COLOR_ATTACHMENT_OPTIMAL转换到PRESENT_SRC_KHR
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (::vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        };

        auto drawFrame = [&]() {
            auto &inFlightFences = frameContext.inFlightFences;
            auto &currentFrame = frameContext.currentFrame;
            auto &presentCompleteSemaphore = frameContext.presentCompleteSemaphore;
            auto &semaphoreIndex = frameContext.semaphoreIndex;
            auto &renderFinishedSemaphore = frameContext.renderFinishedSemaphore;

            auto *device = logical_device_.raw_data();
            while (::vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                                     UINT64_MAX) == VK_TIMEOUT)
                ;
            uint32_t imageIndex;
            VkResult result = ::vkAcquireNextImageKHR(
                device, swapChain, UINT64_MAX, presentCompleteSemaphore[semaphoreIndex],
                VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreateSwapChain();
                return;
            }
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            ::vkResetFences(device, 1, &inFlightFences[currentFrame]);

            auto *commandBuffer = commandBuffers[currentFrame];
            ::vkResetCommandBuffer(commandBuffer, 0);

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

            if (::vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                                inFlightFences[currentFrame]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            VkPresentInfoKHR presentInfo = {.sType = sType<VkPresentInfoKHR>(),
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores =
                                                &renderFinishedSemaphore[imageIndex],
                                            .swapchainCount = 1,
                                            .pSwapchains = &swapChain,
                                            .pImageIndices = &imageIndex};

            result = ::vkQueuePresentKHR(presentQueue, &presentInfo);
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

        // ==================== 用户界面 ====================
        std::cout << "\n=== MVP测试框架 ===" << std::endl;
        std::cout << "功能说明：" << std::endl;
        std::cout << "1. 静态测试：程序启动时运行的数学计算测试" << std::endl;
        std::cout << "2. 动态测试：运行时通过MVP矩阵变化实现的动画" << std::endl;
        std::cout << "3. 3D场景：透视投影，启用深度测试" << std::endl;
        std::cout << "4. 2D UI：正交投影，禁用深度测试，支持alpha混合" << std::endl;
        std::cout << "\n当前启用的动态测试：" << std::endl;
        std::cout << "  - 3D动画: " << (dynamicTests.test3DAnimation ? "✓" : "✗")
                  << std::endl;
        std::cout << "  - 2D动画: " << (dynamicTests.test2DAnimation ? "✓" : "✗")
                  << std::endl;
        std::cout << "  - 相机轨道: " << (dynamicTests.testCameraOrbit ? "✓" : "✗")
                  << std::endl;
        std::cout << "  - UI移动: " << (dynamicTests.testUIMovement ? "✓" : "✗")
                  << std::endl;
        std::cout << "  - 深度动画: " << (dynamicTests.testDepthAnimation ? "✓" : "✗")
                  << std::endl;

        // 在用户界面部分添加深度测试说明
        std::cout << "\n深度测试状态：" << std::endl;
        std::cout << "  - 深度格式: " << depthFormat << std::endl;
        std::cout << "  - 深度附件: "
                  << (depthImageView != VK_NULL_HANDLE ? "已创建" : "未创建")
                  << std::endl;
        std::cout << "  - 深度测试: 启用" << std::endl;
        std::cout << "  - 深度比较: VK_COMPARE_OP_LESS" << std::endl;

        // 在动画部分添加深度测试的视觉反馈
        if (dynamicTests.testDepthAnimation)
        {
            std::cout << "\n深度动画测试：" << std::endl;
            std::cout << "  第一个立方体在Z轴上前后移动" << std::endl;
            std::cout << "  观察深度测试如何决定哪个面可见" << std::endl;
        }

        // 主循环
        while (window.shouldClose() == 0)
        {
            surface::pollEvents();
            drawFrame();
        }

        ::vkDeviceWaitIdle(logical_device_.raw_data());

        // 清理
        if (graphicsPipeline3D != nullptr)
        {
            logical_device_.destroyPipeline(graphicsPipeline3D, nullptr);
        }
        if (pipelineLayout3D != nullptr)
        {
            logical_device_.destroyPipelineLayout(pipelineLayout3D, nullptr);
        }
        if (graphicsPipeline2D != nullptr)
        {
            logical_device_.destroyPipeline(graphicsPipeline2D, nullptr);
        }
        if (pipelineLayout2D != nullptr)
        {
            logical_device_.destroyPipelineLayout(pipelineLayout2D, nullptr);
        }

        cleanupDepthResources();
        cleanupSwapChain();
        logical_device_.destroyCommandPool(commandPool);
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface_);
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "\n程序结束" << std::endl;
    return 0;
} // NOLINTEND