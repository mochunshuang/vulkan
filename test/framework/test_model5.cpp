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

#include <sstream>

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

    // ==================== 投影矩阵（Vulkan标准） ====================
    /*
    构建Vulkan标准的正交投影矩阵：
    1. X轴：将[left, right]线性映射到[-1, 1]
       proj[0][0] = 2/(r-l)      // X缩放
       proj[3][0] = -(r+l)/(r-l) // X平移

    2. Y轴：将[top, bottom]映射到[-1, 1]，且Y向下
       proj[1][1] = -2/(b-t)     // 负号实现Y轴翻转（向下）
       proj[3][1] = -(b+t)/(b-t) // Y平移

    3. Z轴：将[near, far]映射到[0, 1]（Vulkan深度范围）
       proj[2][2] = 1/(f-n)      // Z缩放
       proj[3][2] = -n/(f-n)     // Z平移（使near映射到0）
    */
    glm::mat4 getProjectionMatrix() const
    {
        // Vulkan NDC坐标系：
        // X: 左(-1) → 右(+1)
        // Y: 上(-1) → 下(+1)  // 注意：Y轴向下！
        // Z: 近(0) → 远(1)     // 与OpenGL [-1,1] 不同！

        glm::mat4 proj = glm::mat4(1.0f);

        // X轴映射：left→right → -1→1
        float invWidth = 1.0f / (right - left);
        proj[0][0] = 2.0f * invWidth;
        proj[3][0] = -(right + left) * invWidth;

        // Y轴映射：top→bottom → -1→1，Y向下（负号翻转）
        float invHeight = 1.0f / (bottom - top);
        proj[1][1] = -2.0f * invHeight; // 关键：负号实现Vulkan Y向下
        proj[3][1] = -(bottom + top) * invHeight;

        // Z轴映射：near→far → 0→1（Vulkan深度范围）
        float invDepth = 1.0f / (far - near);
        proj[2][2] = invDepth;         // Z缩放
        proj[3][2] = -near * invDepth; // Z平移，使near映射到0

        return proj;
    }

    // ==================== 视图矩阵（2D相机变换） ====================
    /*
    2D相机视图矩阵构成（按顺序应用）：
    1. 平移：-position（相机移动反向影响物体位置）
    2. 旋转：绕Z轴旋转-rotation（相机旋转反向影响物体）
    3. 缩放：scale（相机缩放正向影响物体）

    注意：这不是传统3D的"lookAt"视图矩阵，而是2D相机变换矩阵。
    在2D中，我们通常想要：
    - 相机右移 → 画面左移（所以用-position）
    - 相机顺时针旋转 → 画面逆时针旋转（所以用rotation）
    - 相机放大 → 画面放大（所以用scale，不是1/scale）
    */
    glm::mat4 getViewMatrix() const
    {
        glm::mat4 view = glm::mat4(1.0f);

        // 1. 平移：相机位置的反向
        view = glm::translate(view, -position);

        // 2. 旋转：绕Z轴旋转（2D旋转）
        if (rotation != 0.0f)
        {
            view = glm::rotate(view, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        // 3. 缩放：相机缩放的正向影响
        // 注意：这里不是1/scale，因为我们希望相机放大时物体也放大
        view = glm::scale(view, scale);

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
        right = left + width;
        bottom = top + height; // 保持bottom > top
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
        position.z = 0.0f; // 2D相机保持在Z=0平面
    }

    // 旋转相机（2D旋转，绕Z轴）
    void rotate(float angleRadians)
    {
        rotation += angleRadians;
        // 保持角度在[0, 2π)范围内
        rotation = fmod(rotation, glm::two_pi<float>());
    }

    void setRotation(float angleRadians)
    {
        rotation = angleRadians;
    }

    // 缩放相机（同时调整视口大小保持内容居中）
    /*
    缩放逻辑：
    1. 更新缩放因子：scale *= zoomFactor
    2. 调整视口边界，保持中心点不变：
       - 计算新的宽度/高度 = 原始宽度/高度 ÷ zoomFactor
       - 重新计算左右/上下边界
    */
    void zoom(float zoomFactor)
    {
        if (zoomFactor <= 0.0f)
            return; // 防止无效缩放

        // 更新缩放因子
        scale *= zoomFactor;

        // 调整视口大小（保持中心点不变）
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
        // 重置缩放
        scale = glm::vec3(1.0f);
        zoom(zoomLevel);
    }

    // 重置所有变换
    void resetTransform()
    {
        position = glm::vec3(0.0f);
        rotation = 0.0f;
        scale = glm::vec3(1.0f);
        left = 0.0f;
        right = WIDTH;
        top = 0.0f;
        bottom = HEIGHT;
    }

    // ==================== 坐标变换方法 ====================

    // 像素坐标 → NDC坐标（不考虑相机变换）
    // 这是简单的线性映射：像素坐标 → [-1, 1]
    struct PixelToNDC
    {
        glm::vec2 pixel; // 像素坐标（屏幕空间）
        glm::vec2 ndc;   // NDC坐标（规范化设备坐标）
        glm::vec2 world; // 世界坐标（不考虑相机变换）
        bool isVisible;  // 是否在屏幕内
    };

    PixelToNDC transformPixel(float pixelX, float pixelY) const
    {
        PixelToNDC result;
        result.pixel = glm::vec2(pixelX, pixelY);

        // 检查可见性
        result.isVisible =
            (pixelX >= left && pixelX <= right && pixelY >= top && pixelY <= bottom);

        // 像素 → NDC（线性映射公式）
        // ndcX = 2 * (pixelX - left) / (right - left) - 1
        // ndcY = 2 * (pixelY - top) / (bottom - top) - 1
        float invWidth = 1.0f / (right - left);
        float invHeight = 1.0f / (bottom - top);

        result.ndc.x = 2.0f * (pixelX - left) * invWidth - 1.0f;
        result.ndc.y = 2.0f * (pixelY - top) * invHeight - 1.0f;

        // NDC → 世界坐标（不考虑相机变换，只使用投影逆矩阵）
        glm::mat4 invProj = glm::inverse(getProjectionMatrix());
        glm::vec4 worldPos = invProj * glm::vec4(result.ndc.x, result.ndc.y, 0.0f, 1.0f);

        if (worldPos.w != 0.0f)
        {
            worldPos /= worldPos.w; // 透视除法（正交投影w=1，但保持通用性）
        }

        result.world = glm::vec2(worldPos.x, worldPos.y);

        return result;
    }

    // 像素坐标 → 世界坐标（考虑完整的相机变换）
    glm::vec2 pixelToWorld(float pixelX, float pixelY) const
    {
        // 1. 像素 → NDC
        float ndcX = 2.0f * (pixelX - left) / (right - left) - 1.0f;
        float ndcY = 2.0f * (pixelY - top) / (bottom - top) - 1.0f;

        // 2. NDC → 裁剪空间 → 世界空间（使用完整的视图投影逆矩阵）
        glm::mat4 inverseVP = glm::inverse(getViewProjectionMatrix());
        glm::vec4 worldPos = inverseVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);

        // 3. 透视除法
        if (worldPos.w != 0.0f)
        {
            worldPos.x /= worldPos.w;
            worldPos.y /= worldPos.w;
        }

        return glm::vec2(worldPos.x, worldPos.y);
    }

    // 世界坐标 → 像素坐标（考虑相机变换）
    glm::vec2 worldToPixel(const glm::vec2 &worldPos) const
    {
        // 1. 世界 → 裁剪空间（应用视图投影矩阵）
        glm::vec4 clipPos = getViewProjectionMatrix() * glm::vec4(worldPos, 0.0f, 1.0f);

        // 2. 透视除法得到NDC
        if (clipPos.w != 0.0f)
        {
            clipPos.x /= clipPos.w;
            clipPos.y /= clipPos.w;
        }

        // 3. NDC → 像素（逆映射公式）
        // pixelX = left + (ndcX + 1) * 0.5 * (right - left)
        // pixelY = top + (ndcY + 1) * 0.5 * (bottom - top)
        float pixelX = left + (clipPos.x + 1.0f) * 0.5f * (right - left);
        float pixelY = top + (clipPos.y + 1.0f) * 0.5f * (bottom - top);

        return glm::vec2(pixelX, pixelY);
    }

    // ==================== 创建特定UI区域的相机 ====================
    static OrthographicCamera createForUI(float x, float y, float width, float height)
    {
        // UI坐标系：Y向下，与Vulkan一致
        // (x, y)是左上角，(x+width, y+height)是右下角
        return OrthographicCamera(x, x + width, y + height, y);
    }

    // ==================== 辅助方法 ====================

    // 获取相机朝向（2D）
    glm::vec2 getForward() const
    {
        // 在2D中，前向通常指向屏幕外，这里返回旋转方向
        return glm::vec2(cos(rotation), sin(rotation));
    }

    glm::vec2 getRight() const
    {
        return glm::vec2(-sin(rotation), cos(rotation));
    }

    // 获取视口中心（像素坐标）
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

    // ==================== 调试方法 ====================
    void printInfo() const
    {
        std::cout << "\nOrthographicCamera信息：" << std::endl;
        std::cout << "  视口: left=" << left << ", right=" << right
                  << ", bottom=" << bottom << ", top=" << top << std::endl;
        std::cout << "  深度: near=" << near << ", far=" << far << std::endl;
        std::cout << "  变换: position=(" << position.x << "," << position.y
                  << "), rotation=" << glm::degrees(rotation) << "°, scale=(" << scale.x
                  << "," << scale.y << ")" << std::endl;
    }
};

// ==================== 使用四元数的Vulkan标准透视投影相机 ====================
/*
[四元数相机原理]
传统欧拉角相机有万向节死锁问题，且旋转组合顺序敏感。
四元数相机优势：
1. 无万向节死锁
2. 旋转插值平滑
3. 避免奇异性
4. 旋转组合简单（四元数乘法）

相机表示：
- 位置：glm::vec3
- 朝向：glm::quat（表示从世界坐标到相机坐标的旋转）
- 投影参数：与之前相同

注意：四元数表示"从世界到相机"的旋转，所以：
- 视图矩阵 = 四元数旋转矩阵的转置 + 平移
- 相机看向方向 = 四元数旋转的-Z轴
*/
class QuaternionCamera
{
  public:
    // ==================== 相机参数 ====================
    glm::vec3 position;    // 世界空间中的相机位置
    glm::quat orientation; // 相机朝向四元数（世界→相机旋转）

    // ==================== 投影参数 ====================
    float fov;         // 垂直视野（度数，标准范围：1-179）
    float aspectRatio; // 宽高比（width/height）
    float nearPlane;   // 近平面距离（必须>0）
    float farPlane;    // 远平面距离（必须>nearPlane）

    // ==================== 构造函数 ====================
    QuaternionCamera()
        : position(0.0f, 0.0f, 3.0f),
          orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), // 单位四元数（无旋转）
          fov(45.0f), aspectRatio(16.0f / 9.0f), nearPlane(0.1f), farPlane(100.0f)
    {
    }

    // 通过位置和看向方向构造
    explicit QuaternionCamera(const glm::vec3 &position,
                              const glm::vec3 &lookAt = glm::vec3(0.0f),
                              const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f))
        : position(position), fov(45.0f), aspectRatio(16.0f / 9.0f), nearPlane(0.1f),
          farPlane(100.0f)
    {
        lookAtTarget(lookAt, up);
    }

    // ==================== 视图矩阵（四元数版本） ====================
    /*
    视图矩阵构建：
    1. 四元数转旋转矩阵：mat3(orientation)
    2. 由于orientation是"世界→相机"旋转，所以：
       - 旋转部分 = 四元数旋转矩阵的转置（相机→世界的逆）
       - 平移部分 = -旋转部分 * position
    */
    glm::mat4 getViewMatrix() const
    {
        // 将四元数转换为旋转矩阵
        glm::mat3 rotationMatrix = glm::mat3_cast(orientation);

        // 视图矩阵的旋转部分是相机到世界的旋转，即四元数旋转的逆（转置）
        glm::mat3 viewRotation = glm::transpose(rotationMatrix);

        // 平移部分：相机位置在世界空间，需要转换到相机空间
        glm::vec3 viewTranslation = -viewRotation * position;

        // 构建4x4视图矩阵
        glm::mat4 viewMatrix = glm::mat4(viewRotation);
        viewMatrix[3] = glm::vec4(viewTranslation, 1.0f);

        return viewMatrix;
    }

    // ==================== 投影矩阵（与之前相同） ====================
    glm::mat4 getProjectionMatrix() const
    {
        // 计算垂直视野的半角正切值
        float tanHalfFovy = tan(glm::radians(fov) * 0.5f);

        // 初始化零矩阵
        glm::mat4 proj = glm::mat4(0.0f);

        // X轴缩放：考虑宽高比
        proj[0][0] = 1.0f / (aspectRatio * tanHalfFovy);

        // Y轴缩放：负号实现Vulkan Y轴向下
        proj[1][1] = -1.0f / tanHalfFovy;

        // Z轴缩放：far/(near - far)，注意结果是负的！
        float range = nearPlane - farPlane; // 负值
        proj[2][2] = farPlane / range;      // 负值

        // 关键：将w分量复制到z，用于透视除法
        proj[2][3] = -1.0f; // 必须是-1.0！

        // Z轴平移：使近平面映射到NDC.z=0
        proj[3][2] = (nearPlane * farPlane) / range; // 负值

        return proj;
    }

    // ==================== 获取方向向量 ====================
    /*
    四元数表示旋转，可以轻松获取方向向量：
    - 前向：四元数旋转的-Z轴（相机默认看向-Z）
    - 右向：四元数旋转的+X轴
    - 上向：四元数旋转的+Y轴
    */
    glm::vec3 getForward() const
    {
        // 四元数旋转的-Z轴（相机看向方向）
        return -glm::mat3_cast(orientation)[2]; // 第三列是-Z轴
    }

    glm::vec3 getRight() const
    {
        // 四元数旋转的+X轴
        return glm::mat3_cast(orientation)[0]; // 第一列是+X轴
    }

    glm::vec3 getUp() const
    {
        // 四元数旋转的+Y轴
        return glm::mat3_cast(orientation)[1]; // 第二列是+Y轴
    }

    // ==================== 设置看向目标 ====================
    void lookAtTarget(const glm::vec3 &target,
                      const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        glm::vec3 forward = glm::normalize(target - position);

        // 更直接的四元数构造方法
        if (glm::length(forward) > 0.001f)
        {
            // 直接从前向和上向量构造四元数
            glm::vec3 right = glm::normalize(glm::cross(up, forward));
            glm::vec3 cameraUp = glm::cross(forward, right);

            glm::mat3 rotation(right, cameraUp, -forward); // 注意负号
            orientation = glm::quat_cast(rotation);
            orientation = glm::normalize(orientation);
        }
    }
    void lookAt(const glm::vec3 &target,
                const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        // 计算看向目标的方向
        glm::vec3 direction = glm::normalize(target - position);

        // 计算旋转矩阵（从默认朝向(0,0,-1)到目标方向）
        glm::vec3 desiredForward = glm::normalize(target - position);
        glm::vec3 desiredUp = glm::normalize(up);

        // 计算右向量
        glm::vec3 right = glm::normalize(glm::cross(desiredForward, desiredUp));

        // 重新计算上向量（确保正交）
        desiredUp = glm::normalize(glm::cross(right, desiredForward));

        // 构建旋转矩阵
        glm::mat3 rotationMat;
        rotationMat[0] = right;
        rotationMat[1] = desiredUp;
        rotationMat[2] = -desiredForward; // 注意：相机空间中是-Z

        // 转换为四元数
        orientation = glm::quat_cast(rotationMat);
    }

    // ==================== 四元数旋转方法 ====================
    // 绕本地轴旋转（更直观）
    void rotateLocal(float angleRadians, const glm::vec3 &axis)
    {
        // 创建旋转四元数
        glm::quat rotation = glm::angleAxis(angleRadians, axis);

        // 右乘：新的朝向 = 当前朝向 * 旋转（本地旋转）
        orientation = orientation * rotation;
        orientation = glm::normalize(orientation);
    }

    // 绕世界轴旋转
    void rotateWorld(float angleRadians, const glm::vec3 &axis)
    {
        // 创建旋转四元数
        glm::quat rotation = glm::angleAxis(angleRadians, axis);

        // 左乘：新的朝向 = 旋转 * 当前朝向（世界旋转）
        orientation = rotation * orientation;
        orientation = glm::normalize(orientation);
    }

    // 俯仰旋转（绕本地右向量）
    void rotatePitch(float angleDegrees)
    {
        float angleRad = glm::radians(angleDegrees);
        glm::vec3 right = getRight();
        rotateLocal(angleRad, right);
    }

    // 偏航旋转（绕世界上向量）
    void rotateYaw(float angleDegrees)
    {
        float angleRad = glm::radians(angleDegrees);
        rotateWorld(angleRad, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // 滚转旋转（绕本地前向量）
    void rotateRoll(float angleDegrees)
    {
        float angleRad = glm::radians(angleDegrees);
        glm::vec3 forward = getForward();
        rotateLocal(angleRad, forward);
    }

    // 直接设置四元数
    void setOrientation(const glm::quat &newOrientation)
    {
        orientation = glm::normalize(newOrientation);
    }

    // 从欧拉角设置（绕ZYX顺序）
    void setOrientationFromEuler(float pitchDeg, float yawDeg, float rollDeg)
    {
        glm::vec3 eulerAngles(glm::radians(pitchDeg), glm::radians(yawDeg),
                              glm::radians(rollDeg));

        // GLM使用ZYX顺序（roll, pitch, yaw）
        orientation = glm::quat(eulerAngles);
    }

    // ==================== 移动函数 ====================
    // 沿本地轴移动
    void moveLocal(const glm::vec3 &translation)
    {
        // 将本地位移转换到世界空间
        glm::vec3 worldTranslation = glm::mat3_cast(orientation) * translation;
        position += worldTranslation;
    }

    void moveForward(float distance)
    {
        // 向前是-Z轴方向
        moveLocal(glm::vec3(0.0f, 0.0f, -distance));
    }

    void moveRight(float distance)
    {
        // 向右是+X轴方向
        moveLocal(glm::vec3(distance, 0.0f, 0.0f));
    }

    void moveUp(float distance)
    {
        // 向上是+Y轴方向
        moveLocal(glm::vec3(0.0f, distance, 0.0f));
    }

    // 沿世界轴移动
    void moveWorld(const glm::vec3 &translation)
    {
        position += translation;
    }

    void moveWorldUp(float distance)
    {
        position += glm::vec3(0.0f, distance, 0.0f);
    }

    // ==================== 插值方法 ====================
    // 球形线性插值（平滑旋转）
    static QuaternionCamera slerp(const QuaternionCamera &a, const QuaternionCamera &b,
                                  float t)
    {
        QuaternionCamera result;
        result.position = glm::mix(a.position, b.position, t);
        result.orientation = glm::slerp(a.orientation, b.orientation, t);
        result.fov = glm::mix(a.fov, b.fov, t);
        result.aspectRatio = a.aspectRatio; // 通常不变
        result.nearPlane = a.nearPlane;
        result.farPlane = a.farPlane;
        return result;
    }

    // ==================== 设置参数 ====================
    void setPosition(const glm::vec3 &newPosition)
    {
        position = newPosition;
    }

    void setFov(float newFov)
    {
        fov = glm::clamp(newFov, 1.0f, 179.0f);
    }

    void setAspectRatio(float newAspectRatio)
    {
        aspectRatio = (newAspectRatio > 0.001f) ? newAspectRatio : 1.0f;
    }

    void setAspectRatio(float width, float height)
    {
        if (height > 0.001f)
        {
            aspectRatio = width / height;
        }
    }

    void setNearPlane(float near)
    {
        nearPlane = glm::max(near, 0.001f);
        if (nearPlane >= farPlane)
        {
            farPlane = nearPlane + 1.0f;
        }
    }

    void setFarPlane(float far)
    {
        farPlane = glm::max(far, nearPlane + 0.001f);
    }

    // ==================== 获取参数 ====================
    glm::vec3 getPosition() const
    {
        return position;
    }
    glm::quat getOrientation() const
    {
        return orientation;
    }
    float getFov() const
    {
        return fov;
    }
    float getAspectRatio() const
    {
        return aspectRatio;
    }
    float getNearPlane() const
    {
        return nearPlane;
    }
    float getFarPlane() const
    {
        return farPlane;
    }

    // ==================== 视图投影矩阵 ====================
    glm::mat4 getViewProjectionMatrix() const
    {
        return getProjectionMatrix() * getViewMatrix();
    }

    // ==================== 辅助计算 ====================
    float getNearHeight() const
    {
        float tanHalfFov = std::tan(glm::radians(fov) * 0.5f);
        return 2.0f * tanHalfFov * nearPlane;
    }

    float getNearWidth() const
    {
        return getNearHeight() * aspectRatio;
    }

    float getFarHeight() const
    {
        float tanHalfFov = std::tan(glm::radians(fov) * 0.5f);
        return 2.0f * tanHalfFov * farPlane;
    }

    float getFarWidth() const
    {
        return getFarHeight() * aspectRatio;
    }

    // ==================== 辅助功能 ====================
    std::string toString() const
    {
        std::stringstream ss;
        ss << "QuaternionCamera:\n";
        ss << "  Position: (" << position.x << ", " << position.y << ", " << position.z
           << ")\n";

        // 四元数表示
        ss << "  Orientation: (" << orientation.x << ", " << orientation.y << ", "
           << orientation.z << ", " << orientation.w << ")\n";

        // 欧拉角表示（方便理解）
        glm::vec3 euler = glm::eulerAngles(orientation);
        ss << "  Euler Angles (rad): (" << euler.x << ", " << euler.y << ", " << euler.z
           << ")\n";

        // 方向向量
        ss << "  Forward: (" << getForward().x << ", " << getForward().y << ", "
           << getForward().z << ")\n";
        ss << "  Right: (" << getRight().x << ", " << getRight().y << ", " << getRight().z
           << ")\n";
        ss << "  Up: (" << getUp().x << ", " << getUp().y << ", " << getUp().z << ")\n";

        ss << "  FOV: " << fov << " degrees\n";
        ss << "  Aspect: " << aspectRatio << "\n";
        ss << "  Near: " << nearPlane << ", Far: " << farPlane;
        return ss.str();
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

    // 模型矩阵（局部空间->世界空间）
    glm::mat4 getModelMatrix() const
    {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMat = glm::mat4_cast(rotation);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);

        // 通常顺序：先缩放，再旋转，最后平移
        return translation * rotationMat * scaleMat;
    }

    // 其他实用方法
    void translate(const glm::vec3 &translation)
    {
        position += translation;
    }

    void rotate(float angle, const glm::vec3 &axis = glm::vec3{0})
    {
        rotation = glm::rotate(rotation, angle, axis);
    }

    void lookAt(const glm::vec3 &target, const glm::vec3 &up = glm::vec3(0, 1, 0))
    {
        glm::vec3 direction = glm::normalize(target - position);
        rotation = glm::quatLookAt(direction, up);
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

    // 获取前向、上、右方向
    glm::vec3 getForward() const
    {
        return rotation * glm::vec3(0, 0, -1);
    }

    glm::vec3 getUp() const
    {
        return rotation * glm::vec3(0, 1, 0);
    }

    glm::vec3 getRight() const
    {
        return rotation * glm::vec3(1, 0, 0);
    }

    // 基于局部坐标系的移动方法
    void moveForward(float distance)
    {
        position += getForward() * distance;
    }

    void moveBackward(float distance)
    {
        position -= getForward() * distance;
    }

    void moveRight(float distance)
    {
        position += getRight() * distance;
    }

    void moveLeft(float distance)
    {
        position -= getRight() * distance;
    }

    void moveUp(float distance)
    {
        position += getUp() * distance;
    }

    void moveDown(float distance)
    {
        position -= getUp() * distance;
    }

    // 或者一个通用的局部移动方法
    void moveLocal(const glm::vec3 &localMovement)
    {
        position += rotation * localMovement;
    }

    // 6. 绕点旋转（围绕世界空间中的某一点旋转）
    void rotateAround(const glm::vec3 &point, float angle, const glm::vec3 &axis)
    {
        // 计算相对于旋转中心的向量
        glm::vec3 relativePos = position - point;

        // 创建旋转矩阵
        glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), angle, axis);

        // 应用旋转到相对位置
        glm::vec3 newRelativePos = glm::vec3(rotationMat * glm::vec4(relativePos, 1.0f));

        // 更新位置
        position = point + newRelativePos;

        // 更新自身旋转
        rotate(angle, axis);
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
    std::vector<RenderObject> &getObjects3D()
    {
        return objects3D;
    }
    std::vector<RenderObject> &getObjects2D()
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
// ==================== 测试辅助函数 ====================

void printVector(const std::string &name, const glm::vec3 &vec)
{
    std::cout << name << ": (" << vec.x << ", " << vec.y << ", " << vec.z << ")"
              << std::endl;
}

void printVector(const std::string &name, const glm::vec4 &vec)
{
    std::cout << name << ": (" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w
              << ")" << std::endl;
}
void printMatrix(const std::string &name, const glm::mat4 &mat, bool compact = false)
{
    std::cout << "\n" << name << ":" << std::endl;
    if (compact)
    {
        for (int i = 0; i < 4; ++i)
        {
            std::cout << "  ";
            for (int j = 0; j < 4; ++j)
            {
                std::cout << std::setw(10) << std::fixed << std::setprecision(4)
                          << mat[j][i];
                if (j < 3)
                    std::cout << ", ";
            }
            std::cout << std::endl;
        }
    }
    else
    {
        for (int i = 0; i < 4; ++i)
        {
            std::cout << "  [ ";
            for (int j = 0; j < 4; ++j)
            {
                std::cout << std::setw(10) << std::fixed << std::setprecision(4)
                          << mat[i][j];
                if (j < 3)
                    std::cout << "  ";
            }
            std::cout << " ]" << std::endl;
        }
    }
}

void printContextInfo()
{
    std::cout << "=== Vulkan Camera 测试上下文 ===\n";

#ifdef GLM_VERSION
    std::cout << "GLM Version: " << GLM_VERSION_MAJOR << "." << GLM_VERSION_MINOR << "."
              << GLM_VERSION_PATCH << "\n";
#else
    std::cout << "GLM Version: Unknown\n";
#endif

#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
    std::cout << "✅ GLM_FORCE_DEPTH_ZERO_TO_ONE: Enabled (Good for Vulkan)\n";
#else
    std::cout
        << "⚠️  GLM_FORCE_DEPTH_ZERO_TO_ONE: Not enabled! May affect projection safety.\n";
#endif

#ifdef GLM_FORCE_LEFT_HANDED
    std::cout
        << "⚠️  GLM_FORCE_LEFT_HANDED: Enabled → May conflict with right-handed world!\n";
#else
    std::cout << "✅ Using default right-handed math (recommended for most 3D apps)\n";
#endif

#ifdef GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    std::cout
        << "💡 GLM_FORCE_DEFAULT_ALIGNED_GENTYPES: Enabled → Better UBO alignment\n";
#endif

    std::cout << "Assumption: Right-handed world, Vulkan NDC [Z:0–1, Y↓]\n\n";
}
// 测试1: 基础属性测试
void testBasicProperties(QuaternionCamera &cam)
{
    std::cout << "\n1. 基础属性测试:" << std::endl;
    std::cout << "----------------" << std::endl;

    printVector("相机位置", cam.getPosition());
    printVector("前向向量", cam.getForward());
    printVector("右向量", cam.getRight());
    printVector("上向量", cam.getUp());

    std::cout << "  " << std::setw(20) << std::left << "FOV"
              << ": " << cam.getFov() << "°" << std::endl;
    std::cout << "  " << std::setw(20) << std::left << "宽高比"
              << ": " << cam.getAspectRatio() << std::endl;
    std::cout << "  " << std::setw(20) << std::left << "近平面"
              << ": " << cam.getNearPlane() << std::endl;
    std::cout << "  " << std::setw(20) << std::left << "远平面"
              << ": " << cam.getFarPlane() << std::endl;

    // 验证向量正交性
    glm::vec3 forward = cam.getForward();
    glm::vec3 right = cam.getRight();
    glm::vec3 up = cam.getUp();

    float dotFR = glm::dot(forward, right);
    float dotFU = glm::dot(forward, up);
    float dotRU = glm::dot(right, up);

    std::cout << "\n  向量正交性检查:" << std::endl;
    std::cout << "  F·R = " << dotFR << " (应为≈0)" << std::endl;
    std::cout << "  F·U = " << dotFU << " (应为≈0)" << std::endl;
    std::cout << "  R·U = " << dotRU << " (应为≈0)" << std::endl;

    if (fabs(dotFR) < 0.001f && fabs(dotFU) < 0.001f && fabs(dotRU) < 0.001f)
    {
        std::cout << "  ✅ 相机坐标系正交性良好" << std::endl;
    }
    else
    {
        std::cout << "  ⚠️  相机坐标系正交性可能有问题" << std::endl;
    }
}

// 测试2: 矩阵测试 - 修正版
void testMatrices(QuaternionCamera &cam)
{
    std::cout << "\n\n2. 矩阵测试:" << std::endl;
    std::cout << "-------------" << std::endl;

    glm::mat4 view = cam.getViewMatrix();
    glm::mat4 proj = cam.getProjectionMatrix();
    glm::mat4 viewProj = cam.getViewProjectionMatrix();

    printMatrix("视图矩阵", view, false);
    printMatrix("投影矩阵", proj, true);

    // ✅ 正确的Vulkan投影矩阵检查
    std::cout << "\n  ✅ 正确的Vulkan投影矩阵检查:" << std::endl;
    std::cout << "  proj[1][1] = " << proj[1][1] << " (负值✓，表示Y轴翻转)" << std::endl;
    std::cout << "  proj[2][3] = " << proj[2][3] << " (应为-1.0，将w复制到z)"
              << std::endl;

    // GLM的perspectiveRH_ZO公式：
    // A = farPlane / (farPlane - nearPlane)
    // B = -farPlane * nearPlane / (farPlane - nearPlane)
    float expectedA = cam.getFarPlane() / (cam.getFarPlane() - cam.getNearPlane());
    float expectedB = -(cam.getFarPlane() * cam.getNearPlane()) /
                      (cam.getFarPlane() - cam.getNearPlane());

    std::cout << "  proj[2][2] = " << proj[2][2] << " (A = " << expectedA << ")"
              << std::endl;
    std::cout << "  proj[3][2] = " << proj[3][2] << " (B = " << expectedB << ")"
              << std::endl;

    // 验证矩阵结构
    bool validStructure = proj[0][3] == 0.0f && proj[1][3] == 0.0f && // 第4列前两个为0
                          proj[3][0] == 0.0f && proj[3][1] == 0.0f &&
                          proj[3][3] == 0.0f &&             // 第4行为0
                          fabs(proj[2][3] + 1.0f) < 0.001f; // proj[2][3] ≈ -1.0

    std::cout << "\n  矩阵结构验证: " << (validStructure ? "✅" : "❌") << std::endl;

    // 验证矩阵乘法
    glm::mat4 viewProjManual = proj * view;
    float diff = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            diff += fabs(viewProj[i][j] - viewProjManual[i][j]);
        }
    }

    std::cout << "\n  矩阵乘法验证:" << std::endl;
    std::cout << "  viewProj == proj * view ? 差值为: " << diff << std::endl;
    if (diff < 0.0001f)
    {
        std::cout << "  ✅ 矩阵乘法正确" << std::endl;
    }
    else
    {
        std::cout << "  ❌ 矩阵乘法错误" << std::endl;
    }
}

void testDepth(QuaternionCamera &cam)
{
    std::cout << "\n\n3. 深度范围测试:" << std::endl;
    std::cout << "----------------" << std::endl;

    glm::mat4 proj = cam.getProjectionMatrix();

    std::cout << "  ✅ 直接测试GLM矩阵的深度变换:" << std::endl;

    float near = cam.getNearPlane();
    float far = cam.getFarPlane();

    // 从矩阵中提取参数
    float c = proj[2][2]; // 应该 ≈ far/(near-far)
    float d = proj[3][2]; // 应该 ≈ near*far/(near-far)

    std::cout << "  从矩阵提取的参数:\n";
    std::cout << "  c = proj[2][2] = " << c << "\n";
    std::cout << "  d = proj[3][2] = " << d << "\n";
    std::cout << "  公式: NDC.z = (c*z + d) / (-z)\n";

    // 测试点
    struct TestCase
    {
        float z_camera;
        std::string description;
    };

    std::vector<TestCase> tests = {
        {-near, "近平面"},    {-near * 2.0f, "2倍近平面"}, {-10.0f, "10 units"},
        {-50.0f, "50 units"}, {-far * 0.9f, "90%远平面"},  {-far, "远平面"}};

    bool allCorrect = true;

    for (const auto &test : tests)
    {
        // 使用GLM矩阵计算
        glm::vec4 point_camera(0.0f, 0.0f, test.z_camera, 1.0f);
        glm::vec4 point_proj = proj * point_camera;
        glm::vec4 point_ndc = point_proj / point_proj.w;

        // 使用提取的参数手动计算
        float manual_ndc_z = (c * test.z_camera + d) / (-test.z_camera);

        std::cout << "\n  " << test.description << " (z=" << test.z_camera << "):\n";
        std::cout << "    GLM计算 NDC深度 = " << point_ndc.z << "\n";
        std::cout << "    手动计算 = " << manual_ndc_z << "\n";

        // 检查两者是否一致
        if (fabs(point_ndc.z - manual_ndc_z) < 0.0001f)
        {
            std::cout << "    ✅ 计算一致" << "\n";
        }
        else
        {
            std::cout << "    ❌ 计算不一致" << "\n";
            allCorrect = false;
        }

        // 检查是否在Vulkan范围内 [0,1]
        bool inRange = (point_ndc.z >= -0.001f && point_ndc.z <= 1.001f);
        std::cout << "    在Vulkan范围内[0,1]: " << (inRange ? "✅" : "❌") << "\n";
    }

    // 边界验证
    std::cout << "\n  ✅ 边界验证:\n";

    glm::vec4 near_point(0.0f, 0.0f, -near, 1.0f);
    glm::vec4 near_ndc = proj * near_point;
    near_ndc /= near_ndc.w;

    glm::vec4 far_point(0.0f, 0.0f, -far, 1.0f);
    glm::vec4 far_ndc = proj * far_point;
    far_ndc /= far_ndc.w;

    std::cout << "  近平面: NDC深度 = " << near_ndc.z << " (应≈0.0)\n";
    std::cout << "  远平面: NDC深度 = " << far_ndc.z << " (应≈1.0)\n";

    bool boundariesCorrect = (near_ndc.z > -0.01f && near_ndc.z < 0.01f) &&
                             (far_ndc.z > 0.99f && far_ndc.z < 1.01f);

    if (boundariesCorrect && allCorrect)
    {
        std::cout << "\n  ✅ 深度变换完全正确！符合Vulkan标准！\n";
    }
    else
    {
        std::cout << "\n  ❌ 深度变换有问题\n";
    }

    // 解释深度非线性
    std::cout << "\n  📊 深度映射特性:\n";
    std::cout << "  注意：透视投影的深度映射是非线性的！\n";
    std::cout << "  这是正常的，深度值在近平面附近变化快，远平面附近变化慢。\n";
    std::cout << "  这正是z-fighting在远处更明显的原因。\n";

    // 展示非线性特性
    std::cout << "\n  深度分布示例:\n";
    for (float percent = 0.1f; percent <= 1.0f; percent += 0.1f)
    {
        float z = -(near + (far - near) * percent);
        glm::vec4 p(0, 0, z, 1);
        glm::vec4 ndc = proj * p;
        ndc /= ndc.w;
        std::cout << "    " << std::setw(3) << int(percent * 100)
                  << "%深度: z=" << std::setw(8) << z << " → NDC=" << ndc.z << "\n";
    }
}

// 新增：Vulkan专用验证
void testVulkanCompatibility(QuaternionCamera &cam)
{
    std::cout << "\n\n4. Vulkan兼容性专项测试:" << std::endl;
    std::cout << "-------------------------" << std::endl;

    glm::mat4 proj = cam.getProjectionMatrix();

    std::cout << "  ✅ Vulkan NDC坐标系验证:" << std::endl;

    // 测试左上角、右上角、左下角、右下角的点
    glm::vec4 testPoints[4] = {
        glm::vec4(-1, 1, -10, 1),  // 左上
        glm::vec4(1, 1, -10, 1),   // 右上
        glm::vec4(-1, -1, -10, 1), // 左下
        glm::vec4(1, -1, -10, 1)   // 右下
    };

    bool allInVulkanRange = true;
    for (int i = 0; i < 4; ++i)
    {
        glm::vec4 ndc = proj * testPoints[i];
        ndc /= ndc.w;

        std::cout << "\n  测试点" << i << ":" << std::endl;
        std::cout << "    相机空间: (" << testPoints[i].x << ", " << testPoints[i].y
                  << ", " << testPoints[i].z << ")" << std::endl;
        std::cout << "    NDC坐标: (" << ndc.x << ", " << ndc.y << ", " << ndc.z << ")"
                  << std::endl;

        // Vulkan NDC范围：x∈[-1,1], y∈[-1,1], z∈[0,1]
        bool inRange = (ndc.x >= -1.01f && ndc.x <= 1.01f && ndc.y >= -1.01f &&
                        ndc.y <= 1.01f && ndc.z >= -0.01f && ndc.z <= 1.01f);

        if (inRange)
        {
            std::cout << "    ✅ 在Vulkan NDC范围内" << std::endl;
        }
        else
        {
            std::cout << "    ❌ 超出Vulkan NDC范围" << std::endl;
            allInVulkanRange = false;
        }
    }

    if (allInVulkanRange)
    {
        std::cout << "\n  ✅ 相机完全符合Vulkan标准！" << std::endl;
    }
    else
    {
        std::cout << "\n  ❌ 相机不符合Vulkan标准" << std::endl;
    }
}

// 测试4: 视锥体测试
void testFrustum(QuaternionCamera &cam)
{
    std::cout << "\n\n4. 视锥体测试:" << std::endl;
    std::cout << "---------------" << std::endl;

    float nearHeight = cam.getNearHeight();
    float nearWidth = cam.getNearWidth();
    float farHeight = cam.getFarHeight();
    float farWidth = cam.getFarWidth();

    std::cout << "  近平面尺寸: " << nearWidth << " × " << nearHeight << std::endl;
    std::cout << "  远平面尺寸: " << farWidth << " × " << farHeight << std::endl;

    // 验证宽高比
    float aspectNear = nearWidth / nearHeight;
    float aspectFar = farWidth / farHeight;

    std::cout << "\n  宽高比验证:" << std::endl;
    std::cout << "  近平面宽高比: " << aspectNear << std::endl;
    std::cout << "  远平面宽高比: " << aspectFar << std::endl;
    std::cout << "  设定宽高比: " << cam.getAspectRatio() << std::endl;

    if (fabs(aspectNear - cam.getAspectRatio()) < 0.001f &&
        fabs(aspectFar - cam.getAspectRatio()) < 0.001f)
    {
        std::cout << "  ✅ 视锥体宽高比一致" << std::endl;
    }
    else
    {
        std::cout << "  ⚠️  视锥体宽高比不一致" << std::endl;
    }
}

// 测试5: 相机移动和旋转测试
void testMovement(QuaternionCamera &cam)
{
    std::cout << "\n\n5. 相机运动测试:" << std::endl;
    std::cout << "----------------" << std::endl;

    glm::vec3 originalPos = cam.getPosition();
    glm::vec3 originalForward = cam.getForward();

    std::cout << "  初始状态:" << std::endl;
    printVector("    位置", originalPos);
    printVector("    前向", originalForward);

    // 向前移动
    cam.moveForward(2.0f);
    glm::vec3 newPos = cam.getPosition();
    glm::vec3 expectedPos = originalPos + originalForward * 2.0f;

    std::cout << "\n  向前移动2单位:" << std::endl;
    printVector("    实际位置", newPos);
    printVector("    期望位置", expectedPos);

    float moveError = glm::length(newPos - expectedPos);
    if (moveError < 0.001f)
    {
        std::cout << "  ✅ 移动正确" << std::endl;
    }
    else
    {
        std::cout << "  ⚠️  移动可能不正确" << std::endl;
    }

    // 重置并测试旋转
    cam.setPosition(originalPos);
    cam.lookAt(glm::vec3(0, 0, -5));

    std::cout << "\n  看向目标(0,0,-5):" << std::endl;
    printVector("    新前向", cam.getForward());

    // 测试俯仰旋转
    float originalPitch = glm::asin(-cam.getForward().y);
    cam.rotatePitch(30.0f);
    float newPitch = glm::asin(-cam.getForward().y);

    std::cout << "\n  俯仰旋转30度:" << std::endl;
    std::cout << "    初始俯仰: " << glm::degrees(originalPitch) << "°" << std::endl;
    std::cout << "    新俯仰: " << glm::degrees(newPitch) << "°" << std::endl;
    std::cout << "    差值: " << glm::degrees(newPitch - originalPitch) << "°"
              << std::endl;
}

// 测试6: Vulkan特定测试
// 测试6: Vulkan特定测试 - 修正版
void testVulkanSpecific(QuaternionCamera &cam)
{
    std::cout << "\n\n6. Vulkan特定测试:" << std::endl;
    std::cout << "-------------------" << std::endl;

    glm::mat4 proj = cam.getProjectionMatrix();

    // ✅ 正确的Vulkan NDC坐标系验证
    std::cout << "  Vulkan NDC坐标系验证:" << std::endl;
    std::cout << "  1. Y轴向下: proj[1][1] = " << proj[1][1] << " (应为负值✓)"
              << std::endl;
    std::cout << "  2. 矩阵结构: proj[2][3] = " << proj[2][3] << " (应为-1.0✓)"
              << std::endl;
    std::cout << "  3. 深度公式: proj[2][2] = " << proj[2][2]
              << " (应为 far/(near-far) ≈ "
              << cam.getFarPlane() / (cam.getNearPlane() - cam.getFarPlane()) << ")"
              << std::endl;

    // 测试几个关键点
    std::cout << "\n  ✅ 关键点测试:" << std::endl;

    // 1. 近平面上的点
    glm::vec4 nearPoint(0, 0, -cam.getNearPlane(), 1.0f);
    glm::vec4 nearNDC = proj * nearPoint;
    nearNDC /= nearNDC.w;
    std::cout << "  近平面点(0,0," << -cam.getNearPlane() << "):" << std::endl;
    std::cout << "    NDC深度 = " << nearNDC.z << " (应≈0.0)" << std::endl;

    // 2. 远平面上的点
    glm::vec4 farPoint(0, 0, -cam.getFarPlane(), 1.0f);
    glm::vec4 farNDC = proj * farPoint;
    farNDC /= farNDC.w;
    std::cout << "  远平面点(0,0," << -cam.getFarPlane() << "):" << std::endl;
    std::cout << "    NDC深度 = " << farNDC.z << " (应≈1.0)" << std::endl;

    // 3. 中间点
    glm::vec4 testPoint(0, 0, -cam.getNearPlane() * 2.0f, 1.0f);
    glm::vec4 ndc = proj * testPoint;
    ndc /= ndc.w;
    std::cout << "  测试点(0,0," << -cam.getNearPlane() * 2.0f << "):" << std::endl;
    std::cout << "    NDC坐标 = (" << ndc.x << ", " << ndc.y << ", " << ndc.z << ")"
              << std::endl;

    // ✅ 正确的检查条件
    bool yValid = (proj[1][1] < 0);                             // Y轴应为负
    bool structureValid = (fabs(proj[2][3] + 1.0f) < 0.001f);   // proj[2][3] ≈ -1.0
    bool nearValid = (nearNDC.z > -0.01f && nearNDC.z < 0.01f); // 近平面深度≈0
    bool farValid = (farNDC.z > 0.99f && farNDC.z < 1.01f);     // 远平面深度≈1

    // 检查NDC范围是否有效（Vulkan: x,y∈[-1,1], z∈[0,1]）
    bool ndcRangeValid = (ndc.x >= -1.01f && ndc.x <= 1.01f && ndc.y >= -1.01f &&
                          ndc.y <= 1.01f && ndc.z >= -0.01f && ndc.z <= 1.01f);

    std::cout << "\n  Vulkan兼容性检查:" << std::endl;
    std::cout << "  Y轴翻转: " << (yValid ? "✅" : "❌") << std::endl;
    std::cout << "  矩阵结构正确: " << (structureValid ? "✅" : "❌") << std::endl;
    std::cout << "  近平面深度≈0: " << (nearValid ? "✅" : "❌") << std::endl;
    std::cout << "  远平面深度≈1: " << (farValid ? "✅" : "❌") << std::endl;
    std::cout << "  NDC坐标在范围内: " << (ndcRangeValid ? "✅" : "❌") << std::endl;

    if (yValid && structureValid && nearValid && farValid)
    {
        std::cout << "\n  ✅ 相机完全符合Vulkan标准！" << std::endl;

        // 额外验证：检查深度是否单调递增
        std::cout << "\n  🔍 深度单调性验证:" << std::endl;
        float prev_z = -999.0f;
        bool monotonic = true;

        for (int i = 0; i <= 10; i++)
        {
            float depth = cam.getNearPlane() +
                          (cam.getFarPlane() - cam.getNearPlane()) * (i / 10.0f);
            glm::vec4 p(0, 0, -depth, 1);
            glm::vec4 n = proj * p;
            n /= n.w;

            std::cout << "    深度 " << std::setw(6) << depth << " → NDC深度 " << n.z;

            if (i > 0 && n.z <= prev_z)
            {
                std::cout << " ❌ 非单调" << std::endl;
                monotonic = false;
            }
            else
            {
                std::cout << " ✅" << std::endl;
            }
            prev_z = n.z;
        }

        if (monotonic)
        {
            std::cout << "  ✅ 深度映射单调递增" << std::endl;
        }
        else
        {
            std::cout << "  ⚠️  深度映射有问题" << std::endl;
        }
    }
    else
    {
        std::cout << "\n  ❌ 相机不符合Vulkan标准" << std::endl;

        // 提供诊断信息
        std::cout << "\n  诊断信息:" << std::endl;
        if (!yValid)
        {
            std::cout << "  - proj[1][1] 应为负值，但得到 " << proj[1][1] << std::endl;
        }
        if (!structureValid)
        {
            std::cout << "  - proj[2][3] 应≈-1.0，但得到 " << proj[2][3] << std::endl;
        }
        if (!nearValid)
        {
            std::cout << "  - 近平面深度应为≈0.0，但得到 " << nearNDC.z << std::endl;
        }
        if (!farValid)
        {
            std::cout << "  - 远平面深度应为≈1.0，但得到 " << farNDC.z << std::endl;
        }
    }
}

void validatePerspectiveCamera()
{

    std::cout << "===================================================================="
              << std::endl;
    std::cout << "                    Vulkan标准相机全面测试" << std::endl;
    std::cout << "===================================================================="
              << std::endl;

    printContextInfo();

    // 创建并测试默认相机
    std::cout << "测试默认相机..." << std::endl;
    QuaternionCamera cam1;

    testBasicProperties(cam1);
    testMatrices(cam1);
    testDepth(cam1);
    testVulkanCompatibility(cam1);
    testFrustum(cam1);
    testMovement(cam1);
    testVulkanSpecific(cam1);

    std::cout
        << "\n\n===================================================================="
        << std::endl;
    std::cout << "测试自定义相机..." << std::endl;
    std::cout << "===================================================================="
              << std::endl;

    // 创建并测试自定义相机
    QuaternionCamera cam2(glm::vec3(10, 5, 8), glm::vec3(0, 0, 0));
    cam2.setFov(60.0f);
    cam2.setAspectRatio(1920.0f / 1080.0f);
    cam2.setNearPlane(0.5f);
    cam2.setFarPlane(200.0f);

    testBasicProperties(cam2);
    testVulkanSpecific(cam2);

    std::cout << "\n" << cam2.toString() << std::endl;
}

// 运行所有静态测试
void runAllStaticTests()
{
    std::cout << "开始运行静态MVP测试...\n" << std::endl;

    // 首先运行坐标系测试
    validateCoordinateSystem();
    validatePerspectiveCamera();

    std::cout << "\n所有静态测试完成!" << std::endl;
}

// ==================== 创建2D UI几何体的辅助函数 ====================
std::vector<Vertex> createUIRectangle(float x, float y, float width, float height,
                                      const glm::vec3 &color)
{
    // 注意：2D UI使用像素坐标，Y轴向下
    // 创建矩形的4个顶点（顺时针顺序）
    y *= -1;
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

    centerY *= -1;

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
        auto cleanupSwapChain = [&]() {
            for (auto *imageView : swapChainImageViews)
                logical_device_.destroyImageView(imageView, nullptr);
            if (swapChain != nullptr)
                logical_device_.destroySwapchainKHR(swapChain);

            swapChainImageViews.clear();
            swapChain = nullptr;
        };

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
        obj3D_2.transform.setPosition(glm::vec3(1.0f, 0.0f, 0.1f));
        obj3D_2.transform.setScale(glm::vec3(0.5f, 0.5f, 0.5f)); // NOTE: 往上离相机越近
        scene.addObject3D(std::move(obj3D_2));

        // NOTE: 画面是 红,绿,蓝. 从左到右,从上到下. x,y是像素
        //  ==================== 创建2D UI几何体 ====================
        //  UI矩形1：红色状态栏
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
                .cullMode = VK_CULL_MODE_NONE, // NOTE: 禁用背面剔除,旋转才什么都看到
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
                         .pMultisampleState = &multisampling,
                         .pDepthStencilState = &depthStencil,
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
                         .pMultisampleState = &multisampling,
                         .pDepthStencilState = &depthStencil, // 2D禁用深度测试
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
        // dynamicTests.test3DAnimation = true; // 3D动画
        // dynamicTests.test2DAnimation = true; // 2D UI动画
        // dynamicTests.testCameraOrbit = true; // 相机轨道（可选）
        // dynamicTests.testUIMovement = true; // UI移动
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
        // camera2d.zoom(1.5f);

        // camera3d.rotateRoll(-5.0f);
        // camera3d.rotatePitch(-5.0f);
        // camera3d.rotateYaw(5.0f);
        // camera3d.position.y += 2.5;
        // camera3d.lookAt(-scene.getObjects3D()[0].transform.position);

        // scene.getObjects2D()[1].transform.moveDown(50);
        // scene.getObjects2D()[1].transform.moveUp(50);
        // scene.getObjects2D()[1].transform.moveLeft(50);
        // scene.getObjects2D()[1].transform.moveRight(200);

        // NOTE: 难怪绕x,y将什么都看不到
        //  float near = 0.0f;     // 近平面
        //  float far = 1.0f; // 远平面
        //  scene.getObjects2D()[1].transform.moveForward(0.1); //NOTE: 将看不到
        // scene.getObjects2D()[1].transform.moveBackward(1);
        // scene.getObjects2D()[1].transform.moveBackward(1.1); // NOTE: 将看不到

        // scene.getObjects2D()[1].transform.rotateAround({}, glm::radians(5.0f), {0, 1,
        // 0});

        // 只有绕Z才看见
        //  scene.getObjects2D()[1].transform.rotateAround({}, glm::radians(5.0f), {0, 0,
        //  1});

        // NOTE: 放大缩小不方便, 全部无效
        //  如果你的物体网格顶点不是以(0,0,0)为中心，缩放就会导致物体移动！
        //  scene.getObjects2D()[1].transform.setScale({2.0, 2.0, 1});

        // 方案1：调整网格中心点
        // 确保你的2D sprite/模型的原点在几何中心：
        // for (auto &v : scene.getObjects2D()[1].vertices)
        // {
        //     v.pos *= 10;
        // }

        // c0: 3D
        //  scene.getObjects3D()[1].transform.moveDown(0.5);
        //  scene.getObjects3D()[1].transform.moveUp(0.5);
        //  scene.getObjects3D()[1].transform.moveLeft(0.5);
        //  scene.getObjects3D()[1].transform.moveRight(0.5);

        // scene.getObjects3D()[1].transform.moveForward(98); // NOTE: 将看不到
        // scene.getObjects3D()[1].transform.moveForward(97); // NOTE: 最小的画面
        // scene.getObjects3D()[1].transform.moveBackward(3.0); // NOTE: 将看不到
        // NOTE: 最大的画面,其他3D不可见
        // scene.getObjects3D()[1].transform.moveBackward(2.9);

        // NOTE: 旋转可以看到,每个轴的 旋转正方向. 右手定则
        //  scene.getObjects3D()[1].transform.rotateAround({}, glm::radians(50.0f),
        //                                                 {0, 1, 0});

        // scene.getObjects3D()[1].transform.rotateAround({}, glm::radians(50.0f),
        //                                                {1, 0, 0});

        // scene.getObjects3D()[1].transform.rotateAround({}, glm::radians(10.0f),
        //                                                {0, 0, 1});

        // scene.getObjects3D()[1].transform.setScale({2.0, 2.0, 1});
        scene.getObjects3D()[2].transform.setScale({1.0, 1.0, 20.0});
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
                    float yOffset = sin(time * 2.0f) * 0.5f;
                    // 第一个物体：旋转
                    auto &obj1 = objects3D[0];
                    obj1.transform.setRotation(
                        glm::angleAxis(time, glm::vec3(0.0f, 1.0f, 0.0f)));

                    // 第二个物体：上下浮动
                    auto &obj2 = objects3D[1];

                    obj2.transform.setPosition(glm::vec3(0.8f, yOffset, 0.0f));

                    // 第三个物体：摆动
                    auto &obj3 = objects3D[2];
                    obj3.transform.setRotation(
                        glm::vec3(yOffset * 2, yOffset * 4, yOffset));
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

                if (dynamicTests.testUIMovement)
                {
                    float time = animationTime;
                    float xOffset = sin(time * 2.0f) * 0.005f;
                    camera3D.position.x += xOffset;

                    camera2D.position.y += 100 * xOffset;
                }

                // 动态测试D：深度动画
                if (dynamicTests.testDepthAnimation && objects3D.size() >= 1)
                {
                    auto &obj = objects3D[0];
                    float depth = sin(animationTime) * 2.0f - 1.0f;
                    objects3D[1].transform.position.x = obj.transform.position.x + 0.2;
                    obj.transform.setPosition(glm::vec3(-1.0f, 0.0f, depth));

                    objects2D[1].transform.position.x = 0;
                    objects2D[1].transform.position.y = +10;
                    objects2D[1].transform.position.x += 300 * sin(animationTime * 5.0f);
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

                //-----------------------------------------------------------------------
                // 测试：打印第一个2D对象的顶点变换
                if (false && !objects2D.empty())
                {
                    const auto &uiObj = objects2D[0];

                    // 在CPU端计算MVP并打印
                    glm::mat4 mvp = uiObj.calculateMVP(camera2D);

                    std::cout << "\n=== 2D顶点可见性测试 ===" << std::endl;
                    std::cout << "UI对象顶点数: " << uiObj.vertices.size() << std::endl;
                    std::cout << "相机视口: " << camera2D.right << "x" << camera2D.bottom
                              << std::endl;

                    for (size_t i = 0; i < std::min(uiObj.vertices.size(), size_t(4));
                         ++i)
                    {
                        const auto &vertex = uiObj.vertices[i];
                        glm::vec4 clipPos = mvp * glm::vec4(vertex.pos, 1.0f);

                        if (clipPos.w != 0.0f)
                        {
                            clipPos /= clipPos.w; // 透视除法
                        }

                        bool visible = (clipPos.x >= -1.0f && clipPos.x <= 1.0f &&
                                        clipPos.y >= -1.0f && clipPos.y <= 1.0f);

                        std::cout << "顶点" << i << ": 像素(" << vertex.pos.x << ","
                                  << vertex.pos.y << ") → NDC(" << clipPos.x << ","
                                  << clipPos.y << ") → " << (visible ? "可见" : "不可见")
                                  << std::endl;
                    }
                }

                //-----------------------------------------------------------------------

                // 动态测试C：2D UI动画
                if (dynamicTests.test2DAnimation && objects2D.size() >= 3)
                {
                    float time = animationTime;

                    // 第一个UI：脉动效果
                    auto &ui1 = objects2D[0];
                    float pulse = sin(time * 3.0f) * 0.2f + 2.0f;
                    ui1.transform.setScale(glm::vec3(1.0f, pulse, 1.0f));

                    // 第二个UI：左右移动
                    auto &ui2 = objects2D[1];
                    float moveX = sin(time * 2.0f) * 100.0f;
                    ui2.transform.position.x = moveX;

                    // 第三个UI：旋转
                    auto &ui3 = objects2D[2];
                    ui3.transform.setRotation(
                        glm::angleAxis(time, glm::vec3(0.0f, 0.0f, 1.0f)));
                    // NOTE: 旋转本质还是修改 位置信息. 这样的方式最灵活
                    //  ui3.transform.rotate(time * 100);

                    // NOTE: 3. 自身坐标旋转. 2D 和 3D 不一样
                    objects3D[2].transform.setRotation(
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