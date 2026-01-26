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

#include <iomanip>

#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>

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
constexpr auto TITLE = "MVP Sandbox Test";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr auto VERT_SHADER_PATH = "shaders/test_model_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_model_frag.spv";

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

// ==================== 标准MVP系统 ====================
/*
[完整的MVP变换流程]

世界空间 (右手坐标系)：
    +Y (上)
    ↑
    |      +Z (屏幕外/向后)
    |     /
    |    /
    |   /
    |  /
    +-------→ +X (右)

↓ 应用视图矩阵 (V)
由 glm::lookAt(position, target, up) 创建

视图空间 (右手坐标系)：
              +Y (相机上)
              ↑
              |
    +Z (相机前) ←------ +X (相机右)
    相机看向方向
    （相机看向 -Z 方向）

↓ 应用投影矩阵 (P)
由 glm::perspective(fov, aspect, near, far) 创建

裁剪空间 / NDC (Vulkan标准)：
    X: [-1, 1]  // 左到右
    Y: [-1, 1]  // 下到上（注意：与屏幕Y轴相反）
    Z: [0, 1]   // 近到远

↓ 视口变换
屏幕空间：
    (0,0) 左上角 → (width-1, height-1) 右下角
    Y轴向下（与NDC Y轴相反）
*/
class QuaternionCamera
{
    /*
// 当前设置：
glm::vec3 position = glm::vec3(0.0f, 0.0f, -3.0f);  // 相机在z=-3
glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);     // 看向原点
glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);         // 上方向是+Y

// 这是标准的右手坐标系：
// - Z轴指向屏幕外
// - X轴向右
// - Y轴向上
*/
  public:
    // ==================== 世界空间中的相机参数 ====================
    // 使用右手坐标系
    glm::vec3 position = glm::vec3(0.0f, 0.0f, -3.0f); // 相机在世界空间中的位置
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);    // 相机看向的世界空间点
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);        // 世界空间中的"上"方向

    // ==================== 投影参数 ====================
    float fov = 45.0f; // 视野角度（垂直方向，单位：度）
    float aspectRatio = static_cast<float>(WIDTH) /
                        static_cast<float>(HEIGHT); // aspectRatio: 宽高比，避免图像拉伸
    float nearPlane = 0.1f;  // 近裁剪平面距离（视图空间中的 -nearPlane）
    float farPlane = 100.0f; // 远裁剪平面距离（视图空间中的 -farPlane）

    // ==================== 视图矩阵 V ====================
    // 将世界坐标转换为视图坐标
    // glm::lookAt 执行以下变换：
    // 1. 平移：将相机从 position 移动到原点
    // 2. 旋转：使：
    //    - 从相机指向 target 的方向变为 -Z 轴（相机看向方向）
    //    - up 方向变为 +Y 轴（相机上方向）
    //    - 通过叉积计算右方向为 +X 轴
    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(position, target, up);
    }

    // ==================== 投影矩阵 P====================
    // 将视图坐标转换为裁剪坐标
    // 创建透视投影，模拟人眼/相机的透视效果
    // GLM_FORCE_DEPTH_ZERO_TO_ONE 确保 Vulkan 的 [0,1] Z范围
    // 注意：GLM_FORCE_DEPTH_ZERO_TO_ONE 确保Z范围[0,1]（Vulkan标准）
    glm::mat4 getProjectionMatrix() const
    {
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }

    // [重要：理解视图空间Z轴方向]
    // 在视图空间中：
    // - 相机在原点(0,0,0)
    // - 相机看向-Z方向
    // - 所以物体必须有负的Z值（在相机前方）才能被看到
    // - 您的物体在Z=0.005，相机在Z=-3
    // - 经过glm::lookAt的变换（包含旋转和平移）：
    //   物体视图Z = - (物体世界Z - 相机世界Z) = - (0.005 - (-3)) = -3.005
    //   这个值是负的，意味着物体在相机前方3.005单位，所以可见！

    // 更新宽高比
    void updateAspectRatio(float width, float height)
    {
        aspectRatio = width / height;
    }

    // ==================== 可见范围计算 ====================

    // 返回在目标平面（target所在平面）上的可见范围
    struct VisibleRange
    {
        float minX; // 左边缘
        float maxX; // 右边缘
        float minY; // 下边缘
        float maxY; // 上边缘
    };

    VisibleRange visible() const
    {
        // 关键：计算相机到目标平面的距离
        // 您的相机在(0,0,-3)，看向(0,0,0)，所以距离=3
        float distance = glm::length(target - position);

        return visibleAtDistance(distance);
    }

    // 在任意距离平面上的可见范围
    VisibleRange visibleAtDistance(float distanceFromCamera) const
    {
        // 确保距离在合理范围内
        distanceFromCamera = glm::clamp(distanceFromCamera, nearPlane, farPlane);

        // 核心计算
        float halfFovRad = glm::radians(fov) / 2.0f;
        float tanHalfFov = tan(halfFovRad);

        float halfHeight = distanceFromCamera * tanHalfFov; // Y轴半高
        float halfWidth = halfHeight * aspectRatio;         // X轴半宽

        return {
            .minX = -halfWidth,  // 左边缘
            .maxX = halfWidth,   // 右边缘
            .minY = -halfHeight, // 下边缘
            .maxY = halfHeight   // 上边缘
        };
    }

    // 在Z=0平面上的可见范围（您的物体都在这个平面）
    VisibleRange visibleAtZeroZ() const
    {
        // 计算从相机到Z=0平面沿视线方向的距离
        glm::vec3 viewDir = glm::normalize(target - position);

        if (abs(viewDir.z) < 0.0001f)
        {
            // 如果视线平行于Z=0平面，使用到目标的距离
            return visible();
        }

        // 计算交点：position + t * viewDir 在Z=0
        float t = -position.z / viewDir.z;

        // 确保在近远平面之间
        t = glm::clamp(t, nearPlane, farPlane);

        return visibleAtDistance(t);
    }

    struct DepthMapping
    {
        float linearNear; // 线性近平面距离
        float linearFar;  // 线性远平面距离
        float ndcNear;    // NDC空间近平面值（Vulkan: 0.0）
        float ndcFar;     // NDC空间远平面值（Vulkan: 1.0）
    };

    DepthMapping getDepthMapping() const
    {
        return {.linearNear = nearPlane,
                .linearFar = farPlane,
                .ndcNear = 0.0f, // Vulkan的NDC Z范围是[0, 1]
                .ndcFar = 1.0f};
    }

    // ==================== 坐标变换分析工具函数 ====================
    struct CoordinateAnalysis
    {
        glm::vec3 world; // 世界空间坐标
        glm::vec3 view;  // 视图空间坐标
        glm::vec4 clip;  // 裁剪空间坐标（包含w分量）
        glm::vec3 ndc;   // NDC坐标
        bool isVisible;  // 是否在视锥体内
        float w;         // 透视投影的w分量（用于透视除法）

        // 重载输出运算符
        friend std::ostream &operator<<(std::ostream &os,
                                        const CoordinateAnalysis &analysis)
        {
            os << "=== 坐标分析 ===" << std::endl;
            os << std::fixed << std::setprecision(3);

            // 世界空间
            os << "世界空间: (" << analysis.world.x << ", " << analysis.world.y << ", "
               << analysis.world.z << ")" << std::endl;

            // 视图空间
            os << "视图空间: (" << analysis.view.x << ", " << analysis.view.y << ", "
               << analysis.view.z << ")";

            // 添加视图空间Z值的特殊注释
            if (analysis.view.z < -0.001f)
                os << " [Z<0: 在相机前方 " << -analysis.view.z << " 单位]" << std::endl;
            else if (analysis.view.z > 0.001f)
                os << " [Z>0: 在相机后方 " << analysis.view.z << " 单位]" << std::endl;
            else
                os << " [Z≈0: 在相机平面上]" << std::endl;

            // 裁剪空间（修复：显示完整的4D坐标）
            os << "裁剪空间: (" << analysis.clip.x << ", " << analysis.clip.y << ", "
               << analysis.clip.z << ", w=" << analysis.clip.w << ")" << std::endl;

            // NDC空间
            os << "NDC空间:  (" << analysis.ndc.x << ", " << analysis.ndc.y << ", "
               << analysis.ndc.z << ")" << std::endl;

            // w分量信息
            os << "w分量:    " << analysis.w
               << (analysis.w > 0   ? " (正w，正常透视)"
                   : analysis.w < 0 ? " (负w，在相机后方)"
                                    : " (w=0)")
               << std::endl;

            // 可见性
            os << "可见性:   " << (analysis.isVisible ? "✓ 可见" : "✗ 不可见");

            // 如果不可见，显示原因
            if (!analysis.isVisible)
            {
                os << " [原因: ";
                std::vector<std::string> reasons;

                if (analysis.ndc.x < -1.0f)
                    reasons.push_back("X<-1(屏幕左)");
                else if (analysis.ndc.x > 1.0f)
                    reasons.push_back("X>1(屏幕右)");

                if (analysis.ndc.y < -1.0f)
                    reasons.push_back("Y<-1(屏幕下)");
                else if (analysis.ndc.y > 1.0f)
                    reasons.push_back("Y>1(屏幕上)");

                if (analysis.ndc.z < 0.0f)
                    reasons.push_back("Z<0(近平面前)");
                else if (analysis.ndc.z > 1.0f)
                    reasons.push_back("Z>1(远平面后)");

                // 负w值也导致不可见（在透视除法前检查）
                if (analysis.w <= 0.0f)
                    reasons.push_back("w<=0(在相机后)");

                for (size_t i = 0; i < reasons.size(); ++i)
                {
                    if (i > 0)
                        os << ", ";
                    os << reasons[i];
                }
                os << "]";
            }
            os << std::endl;
            os << "=================" << std::endl;

            return os;
        }
    };

    CoordinateAnalysis analyzePoint(const glm::vec3 &worldPoint) const
    {
        CoordinateAnalysis result;
        result.world = worldPoint;

        // 视图空间变换
        glm::mat4 viewMat = getViewMatrix();
        glm::vec4 viewPos = viewMat * glm::vec4(worldPoint, 1.0f);
        result.view = glm::vec3(viewPos);

        // 裁剪空间变换
        glm::mat4 projMat = getProjectionMatrix();
        glm::vec4 clipPos = projMat * viewPos;
        result.clip = clipPos; // 保存完整的4D坐标
        result.w = clipPos.w;  // 保存w分量

        // NDC空间（透视除法）
        if (abs(clipPos.w) > 0.0001f) // 避免除零
        {
            result.ndc = glm::vec3(clipPos.x, clipPos.y, clipPos.z) / clipPos.w;
        }
        else
        {
            result.ndc = glm::vec3(0.0f);
        }

        // 可见性检查（考虑w分量）
        // 注意：在透视投影中，如果w <= 0，表示点在相机后方或平面上
        bool isInNDC =
            (result.ndc.x >= -1.0f && result.ndc.x <= 1.0f && result.ndc.y >= -1.0f &&
             result.ndc.y <= 1.0f && result.ndc.z >= 0.0f && result.ndc.z <= 1.0f);

        // 还要检查w分量（透视除法前）
        bool hasValidW = (result.w > 0.0f);

        result.isVisible = isInNDC && hasValidW;

        return result;
    }

    // ==================== 视图空间方向获取 ====================
    // 获取相机在视图空间中的各轴方向（用于调试和理解）
    struct ViewSpaceDirections
    {
        glm::vec3 forward; // 相机看向方向（-Z轴）
        glm::vec3 right;   // 相机右侧（+X轴）
        glm::vec3 up;      // 相机上方（+Y轴）
    };

    ViewSpaceDirections getViewSpaceDirections() const
    {
        // 视图矩阵的逆变换的前3列给出视图空间到世界空间的变换
        // 我们可以提取视图空间基向量在世界空间中的表示
        glm::mat4 viewMat = getViewMatrix();
        glm::mat4 invView = glm::inverse(viewMat);

        ViewSpaceDirections dirs;
        dirs.forward = -glm::vec3(invView[2]); // 第三列是Z轴
        dirs.right = glm::vec3(invView[0]);    // 第一列是X轴
        dirs.up = glm::vec3(invView[1]);       // 第二列是Y轴

        return dirs;
    }
};

// 物体变换 - 提供模型矩阵
struct Transform
{
    // mvp: [物体变换参数]
    // position: 物体在世界空间中的位置
    // rotation: 物体的旋转（使用四元数避免万向锁）
    // scale: 物体的缩放
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::identity<glm::quat>();
    glm::vec3 scale = glm::vec3(1.0f);

    // mvp: [模型矩阵] 将模型坐标转换为世界坐标
    // 模型矩阵 = 平移 * 旋转 * 缩放
    // 注意：矩阵乘法顺序很重要！通常是先缩放，再旋转，最后平移
    // 但在GLM中，矩阵乘法是从右到左，所以写的时候顺序是反的
    glm::mat4 getModelMatrix() const
    {
        // mvp: 创建平移矩阵
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        // mvp: 将四元数旋转转换为旋转矩阵
        glm::mat4 rotationMat = glm::mat4_cast(rotation);
        // mvp: 创建缩放矩阵
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
        // mvp: 矩阵组合：translation * rotationMat * scaleMat
        // 这意味着先缩放，然后旋转，最后平移
        return translation * rotationMat * scaleMat;
    }

    // 直接设置变换
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

// 渲染物体 - 独立的渲染单元
class RenderObject
{
  public:
    Transform transform;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    RenderObject(std::vector<Vertex> verts, std::vector<uint32_t> idxs)
        : vertices(std::move(verts)), indices(std::move(idxs))
    {
    }

    // mvp: [计算MVP矩阵] 这是整个系统的核心函数
    // 它将三个变换矩阵组合起来：Projection * View * Model
    // 这个矩阵将顶点从模型空间直接变换到裁剪空间
    glm::mat4 calculateMVP(const QuaternionCamera &camera) const
    {
        // mvp: 获取模型矩阵（模型空间 -> 世界空间）
        glm::mat4 model = transform.getModelMatrix();
        // mvp: 获取视图矩阵（世界空间 -> 视图空间）
        glm::mat4 view = camera.getViewMatrix();
        // mvp: 获取投影矩阵（视图空间 -> 裁剪空间）
        glm::mat4 proj = camera.getProjectionMatrix();
        // mvp: 组合矩阵：proj * view * model
        // 这个矩阵乘法顺序非常重要！
        // 数学上：ClipSpace = P * V * M * ModelSpace
        // 所以矩阵乘法要从右到左：先M，再V，最后P
        return proj * view * model;
    }
};

// 场景管理器
class Scene
{
  private:
    std::vector<RenderObject> objects;
    QuaternionCamera camera;

  public:
    void addObject(RenderObject &&obj)
    {
        objects.push_back(std::move(obj));
    }

    const std::vector<RenderObject> &getObjects() const
    {
        return objects;
    }
    std::vector<RenderObject> &getObjects()
    {
        return objects;
    }
    QuaternionCamera &getCamera()
    {
        return camera;
    }
    const QuaternionCamera &getCamera() const
    {
        return camera;
    }

    // 清除所有物体
    void clearObjects()
    {
        objects.clear();
    }

    // 获取物体数量
    size_t objectCount() const
    {
        return objects.size();
    }
};

// mvp: [Uniform Buffer对象] 用于将MVP矩阵传递给着色器
// 注意：GLM矩阵默认是列主序，这与OpenGL/Vulkan一致
// alignas(16)：确保内存对齐，这是Vulkan的要求
struct UniformBufferObject
{
    alignas(16) glm::mat4 mvp;
};

// 网格数据 - 管理Vulkan资源
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

  public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    MeshData(physical_device &physicalDevice, logical_device &device, VkQueue queue,
             VkCommandPool commandPool, const std::vector<Vertex> &vertices,
             const std::vector<uint32_t> &indices)
        : physicalDevice(physicalDevice), device(device), queue(queue),
          commandPool(commandPool), vertices(vertices), indices(indices)
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

    // mvp: [更新Uniform Buffer] 将计算好的MVP矩阵上传到GPU
    // 这是每一帧都需要做的操作，因为MVP矩阵可能会变化
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

VkPipelineLayout MeshData::pipelineLayout = nullptr;

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

int main()
{
    try
    {
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
                                    .pApplicationName = "MVP Sandbox",
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

        // 创建场景和对象
        Scene scene;

        // 获取可见范围
        auto range = scene.getCamera().visible(); // 在目标平面（Z=0）上的范围
        auto rangez = scene.getCamera().visibleAtZeroZ();
        std::cout << "屏幕可见范围：" << std::endl;
        std::cout << "X: [" << range.minX << ", " << range.maxX << "]" << std::endl;
        std::cout << "Y: [" << range.minY << ", " << range.maxY << "]" << std::endl;
        std::cout << "zX: [" << rangez.minX << ", " << rangez.maxX << "]" << std::endl;
        std::cout << "zY: [" << rangez.minY << ", " << rangez.maxY << "]" << std::endl;

        auto deprange = scene.getCamera().getDepthMapping();
        std::cout << "Near,Far: [" << deprange.linearNear << ", " << deprange.linearFar
                  << "]" << std::endl;

        // mvp: [创建基础的四边形几何体]
        // 这个四边形中心在 (0,0,0)，范围是 [-0.5, 0.5] 的正方形
        std::vector<Vertex> baseVertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // 左下，红色
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // 右下，绿色
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},   // 右上，蓝色
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}}   // 左上，黄色
        };
        std::vector<uint32_t> baseIndices = {0, 1, 2, 2, 3, 0};

        // c0: 测试 setPosition
        //  mvp: [创建多个对象，每个都有独立的位置]
        //  注意：物体的z坐标都是0，而相机在z=-3
        //  根据右手坐标系：相机看向-z方向，所以能看到z=0的物体
        RenderObject obj1(baseVertices, baseIndices);
        // obj1.transform.setPosition(glm::vec3(0.0f, 0.0f, 0.0f)); // 中心
        obj1.transform.setPosition(glm::vec3(1.0f, 0.0f, 0.0f)); // 屏幕左侧
        // obj1.transform.setPosition(glm::vec3(-1.0f, 0.0f, 0.0f)); // 屏幕右侧
        // obj1.transform.setPosition(glm::vec3(range.maxX, 0.0f, 0.0f)); // 屏幕左
        // obj1.transform.setPosition(glm::vec3(range.minX, 0.0f, 0.0f)); // 右侧边界
        // obj1.transform.setPosition(glm::vec3(0.0f, range.minY, 0.0f)); // 屏幕上
        // obj1.transform.setPosition(glm::vec3(0.0f, range.maxY, 0.0f)); // 屏幕下

        auto cp = scene.getCamera().position.z;
        // 在相机前方1单位,很大. 近大远小
        // obj1.transform.setPosition(glm::vec3(0.0f, 0.0f, cp + 1));
        // 与相机重合.什么都看不到
        // obj1.transform.setPosition(glm::vec3(0.0f, 0.0f, cp));

        // NOTE: 最远可以看见的
        // obj1.transform.setPosition(glm::vec3(0.0f, 0.0f, cp + deprange.linearFar));
        // 看不到了
        // obj1.transform.setPosition(
        //     glm::vec3(0.0f, 0.0f, cp + deprange.linearFar + 0.001));

        // NOTE: 占领整个屏幕,看到最近的
        // obj1.transform.setPosition(glm::vec3(0.0f, 0.0f, cp + deprange.linearNear));
        // 再近再也看不到了
        obj1.transform.setPosition(
            glm::vec3(0.0f, 0.0f, cp + deprange.linearNear - 0.001));

        std::cout << scene.getCamera().analyzePoint(
                         glm::vec3(0.0f, 0.0f, cp + deprange.linearNear - 0.001))
                  << '\n';

        // NOTE: 能看到 Z=0.005 的原因
        /*
        物体在世界空间：Z=0.005
        相机在世界空间：Z=-3
        视图变换后，物体在视图空间的Z坐标 = 0.005 - (-3) = 3.005

        在视图空间中，相机看向-Z方向，所以物体Z=3.005意味着它在相机后方3.005单位

        但是！glm::lookAt会旋转整个坐标系，使相机看向-Z。实际上物体是在相机前方被看到的。

        这个看似矛盾的结果是因为您只考虑了平移，没考虑旋转。完整的视图矩阵包含旋转和平移，将世界坐标系重新对齐到相机坐标系。
        如果您想要更直观的坐标系，可以考虑使用左手坐标系（DirectX风格），这样相机看向+Z方向会更直观。但您当前的代码是标准的OpenGL/Vulkan右手坐标系做法，是正确的。
        */
        obj1.transform.setPosition(glm::vec3(0.0f, 0.0f, 0.005));

        std::cout << scene.getCamera().analyzePoint(glm::vec3(0.0f, 0.0f, 0.005)) << '\n';

        auto analysis = scene.getCamera().analyzePoint(glm::vec3(0, 0, -2.901));
        std::cout << analysis;
        auto analysis2 = scene.getCamera().analyzePoint(glm::vec3(0, 0, 0.005));
        std::cout << analysis2;

        // 叠加
        // obj1.transform.position += glm::vec3(1, 0, 0); // 向左移动1
        // obj1.transform.position += glm::vec3(0, 1, 0); // 向下移动1，现在在(1,1,0)

        // obj1.transform.setPosition(glm::vec3(1.0f, 1.0f, 0)); //一步到位

        // c0: 测试 setScale
        // obj1.transform.setScale(glm::vec3(1.0f, 2.0f, 1.0f)); // 长拉伸
        // obj1.transform.setScale(glm::vec3(2.0f, 1.0f, 1.0f)); // 宽拉伸
        // NOTE: 平面缩放 Z 轴没有意义. 什么时候Z轴缩放会有视觉变化？
        // 平面几何体（所有顶点Z值相同）：不会有效果
        // 3D几何体（顶点Z值不同）：有视觉变形效果
        // obj1.transform.setScale(glm::vec3(1.0f, 1.0f, 100.0f));

        // 叠加
        // obj1.transform.setScale(glm::vec3(2.0f, 2.0f, 100.0f));

        // c0: 测试 setRotation: 右手系.大拇指指向正方向
        // 绕Z轴 旋转30度
        // obj1.transform.setRotation(
        //     glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
        // 绕X轴 旋转45度
        // obj1.transform.setRotation(
        //     glm::angleAxis(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
        // 绕Y轴 旋转45度
        // obj1.transform.setRotation(
        //     glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

        /*
        "左边的最后执行，右边的最先执行"
        "代码：X * Y，实际：先Y后X"
        */
        // auto rotation =
        //     glm::angleAxis(glm::radians(30.0f), glm::vec3(1, 0, 0))    // 这个后执行
        //     * glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0)); // 这个先执行
        // obj1.transform.setRotation(rotation);

        static constexpr auto p2 = glm::vec3(-1.0f, 0.0f, 0.0f);
        RenderObject obj2(baseVertices, baseIndices);
        obj2.transform.setPosition(p2);                       // 右边
        obj2.transform.setScale(glm::vec3(0.5f, 0.5f, 1.0f)); // 缩小1/4
        auto set_color = [](std::vector<Vertex> &vertices, glm::vec3 new_color) {
            for (auto &v : vertices)
            {
                v.color = new_color;
            }
        };
        set_color(obj2.vertices, glm::vec3{1.0, 0.0, 0.0});

        RenderObject obj3(baseVertices, baseIndices);
        obj3.transform.setPosition(glm::vec3(0.5f, 0.5f, 0.0f)); // 第一个矩形的左下角
        obj3.transform.setScale(glm::vec3(0.4f, 0.4f, 0));       // 缩小
        obj3.transform.setRotation(
            glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f))); // 旋转30度
        set_color(obj3.vertices, glm::vec3{0.0, 1.0, 0.0});

        RenderObject obj4(baseVertices, baseIndices);
        obj4.transform.setPosition(glm::vec3(0.5f, -0.5f, 0.0f)); // 左上角
        obj4.transform.setScale(glm::vec3(0.1f, 0.1f, 0.f));      // 缩小
        obj4.transform.setRotation(
            glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f))); // 旋转45度
        set_color(obj4.vertices, glm::vec3{0.0, 0.0, 1.0});

        static constexpr auto p5 = glm::vec3(0.5f, -0.5f, 0.0f);
        RenderObject obj5(baseVertices, baseIndices);
        obj5.transform.setPosition(p5);                      // 左上角，与obj4重叠
        obj5.transform.setScale(glm::vec3(0.1f, 0.1f, 0.f)); // 缩小
        set_color(obj5.vertices, glm::vec3{0.0, 1.0, 1.0});
        obj5.transform.setRotation(
            glm::angleAxis(glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 1.0f))); // 旋转45度

        scene.addObject(std::move(obj1));
        scene.addObject(std::move(obj2));
        scene.addObject(std::move(obj3));
        scene.addObject(std::move(obj4));
        scene.addObject(std::move(obj5));

        // 为每个渲染对象创建MeshData
        std::vector<std::unique_ptr<MeshData>> meshDatas;
        for (const auto &obj : scene.getObjects())
        {
            meshDatas.push_back(
                std::make_unique<MeshData>(physicalDevice, logical_device_, graphicsQueue,
                                           commandPool, obj.vertices, obj.indices));
        }

        // c0: 上面的变换全是 P 的变换. 还能变换 M V
        /*
        相机变换在视图矩阵（V）：glm::lookAt(position, target, up)
        物体变换在模型矩阵（M）：getModelMatrix()
        最终变换：MVP = P × V × M
        */
        // 位置（Position）
        auto &camera = scene.getCamera();
        // camera.position.z += 1; // 相机越远（物体变小）,.当前是放大画面
        // camera.position.z -= 1;//缩小. 近大远小
        // camera.position.x += 2; // 往左移动,再看不变的物体. 左边的边框变大,右边的变小
        // camera.position.x -= 2; // 往右移动
        // camera.position.y += 4; // 往下移动,然后仰视
        // camera.position.y -= 4; // 在上面,往下俯视

        // 目标点（Target）
        // 当前：看向世界原点(0,0,0)
        /*
        改变相机的观察方向
        配合position可以创建"观察某点"的效果
        */
        // NOTE:有倾斜角度的效果. 因为正Z轴和世界空间,不再共线. 视图空间和世界空间映射倾斜
        // camera.target = p5; // obj5 将处于屏幕中心
        // camera.target = p2; // obj2 将处于屏幕中心

        // 3. 上方向（Up）
        // camera.up = glm::vec3(0.0f, -1.0f, 0.0f); // NOTE: 上下左右 颠倒,不仅仅Y颠倒

        // 4. 投影参数
        // 增大FOV
        // camera.fov = 90.0f; // 物体变远,能看到更多

        // 减小FOV
        // camera.fov = 20.0f; // 物体变大,能看到更少

        // 5. 改变裁剪平面
        // // 减小近平面（可以看到更近的物体）
        // camera.nearPlane = 0.01f;
        // // 增大远平面（可以看到更远的物体）
        // camera.farPlane = 1000.0f;

        // 创建图形管线
        auto createGraphicsPipeline = [&]() {
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
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

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
                meshDatas[0]->getDescriptorSetLayout();
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
                         .pColorBlendState = &colorBlending,
                         .pDynamicState = &dynamicState,
                         .layout = pipelineLayout,
                         .renderPass = VK_NULL_HANDLE},
                        {.colorAttachmentCount = 1,
                         .pColorAttachmentFormats = &swapChainImageFormat}};

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

        auto [pipelineLayout, graphicsPipeline] = createGraphicsPipeline();

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
        };

        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame,
                                       uint32_t
                                           imageIndex) { // diff:[new] 添加imageIndex参数
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            VkImage image =
                swapChainImages[imageIndex]; // diff:[new] 使用正确的imageIndex
            VkImageView imageView =
                swapChainImageViews[imageIndex]; // diff:[new] 使用正确的imageIndex

            if (::vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // diff:[new] 修复图像布局转换链
            // 1. 从UNDEFINED转换到COLOR_ATTACHMENT_OPTIMAL
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.1F, 0.1F, 0.1F, 1.0F}}};

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

            ::vkCmdBeginRendering(commandBuffer, &renderingInfo);
            ::vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                graphicsPipeline);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            ::vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            ::vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // ==================== 渲染循环的核心部分 ====================
            // mvp: [渲染所有物体] 这是整个MVP系统的执行阶段
            // 对于场景中的每个物体：
            // 1. 计算该物体的MVP矩阵
            // 2. 将MVP矩阵上传到Uniform Buffer
            // 3. 绑定该物体的资源
            // 4. 绘制该物体
            const auto &camera = scene.getCamera();
            std::vector<RenderObject> &objects = scene.getObjects();

            for (size_t i = 0; i < objects.size(); i++)
            {
                if (i < meshDatas.size())
                {
                    auto &currentTransform = objects[i].transform;
                    // currentTransform.setRotation(
                    //     glm::angleAxis(glm::radians(static_cast<float>((i + 1) *
                    //     10)),
                    //                    glm::vec3(0.0f, 0.0f, 1)));

                    // ==================== 平移 ====================
                    // // 方法1：水平排列（X轴方向）
                    // float spacing = 1.5f; // 物体之间的间距
                    // float startX = -static_cast<float>(i) * spacing / 2.0f;
                    // float posX = startX + i * spacing;

                    // 设置位置：在X轴上等间距排列，Y和Z为0
                    // currentTransform.setPosition(glm::vec3(posX, 0.0f, 0.0f));

                    // ==================== 缩放 ====================
                    // 设置缩放，每个物体不同大小
                    float baseScale = 0.5f;
                    float scaleFactor = 1.0f + i * 0.1f; // 逐渐增大
                    // currentTransform.setScale(glm::vec3(baseScale * scaleFactor));

                    // mvp: [计算当前物体的MVP矩阵]
                    // 这是关键的一步：每个物体独立计算自己的MVP矩阵
                    // 这允许每个物体有自己的位置、旋转和缩放
                    glm::mat4 mvp = objects[i].calculateMVP(camera);

                    // mvp: [更新Uniform Buffer]
                    // 将计算好的MVP矩阵上传到GPU，供顶点着色器使用
                    meshDatas[i]->updateUniformBuffer(currentFrame, mvp);

                    // mvp: [绑定并绘制]
                    // 绑定该物体的顶点缓冲区和描述符集
                    // 然后绘制该物体的所有三角形
                    meshDatas[i]->bind(commandBuffer, currentFrame);
                    meshDatas[i]->draw(commandBuffer);
                }
            }
            // ==================== 渲染循环结束 ====================

            ::vkCmdEndRendering(commandBuffer);

            // diff:[new] 2. 从COLOR_ATTACHMENT_OPTIMAL转换到PRESENT_SRC_KHR
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

            // diff:[new] 传递正确的imageIndex
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

        // 主循环
        while (window.shouldClose() == 0)
        {
            surface::pollEvents();
            drawFrame();
        }

        ::vkDeviceWaitIdle(logical_device_.raw_data());

        // 清理
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

        cleanupSwapChain();
        logical_device_.destroyCommandPool(commandPool);
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface_);
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "main done\n";
    return 0;
} // NOLINTEND