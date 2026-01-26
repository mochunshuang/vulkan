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
#include <memory>
#include <unordered_map>

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

// ==================== 投影系统抽象 ====================
class Projection
{
  public:
    virtual ~Projection() = default;
    virtual glm::mat4 getMatrix() const = 0;
    virtual float getAspectRatio() const = 0;
    virtual void updateAspectRatio(float width, float height) = 0;
    virtual std::string getType() const = 0;
};

class PerspectiveProjection : public Projection
{
  private:
    float fov_;
    float aspect_;
    float near_;
    float far_;

  public:
    PerspectiveProjection(float fov = 45.0f, float aspect = 1.0f, float near = 0.1f,
                          float far = 100.0f)
        : fov_(fov), aspect_(aspect), near_(near), far_(far)
    {
    }

    glm::mat4 getMatrix() const override
    {
        return glm::perspective(glm::radians(fov_), aspect_, near_, far_);
    }

    float getAspectRatio() const override
    {
        return aspect_;
    }

    void updateAspectRatio(float width, float height) override
    {
        aspect_ = width / height;
    }

    std::string getType() const override
    {
        return "Perspective";
    }

    // Getters and setters
    void setFov(float fov)
    {
        fov_ = fov;
    }
    void setNearFar(float near, float far)
    {
        near_ = near;
        far_ = far;
    }
    float getFov() const
    {
        return fov_;
    }
    float getNear() const
    {
        return near_;
    }
    float getFar() const
    {
        return far_;
    }
};

class OrthographicProjection : public Projection
{
  private:
    float left_;
    float right_;
    float bottom_;
    float top_;
    float near_;
    float far_;
    float aspect_;

  public:
    OrthographicProjection(float left = -1.0f, float right = 1.0f, float bottom = -1.0f,
                           float top = 1.0f, float near = 0.0f, float far = 1.0f)
        : left_(left), right_(right), bottom_(bottom), top_(top), near_(near), far_(far)
    {
        updateAspect();
    }

    glm::mat4 getMatrix() const override
    {
        return glm::ortho(left_, right_, bottom_, top_, near_, far_);
    }

    float getAspectRatio() const override
    {
        return aspect_;
    }

    void updateAspectRatio(float width, float height) override
    {
        // 保持视觉范围，调整边界
        float newAspect = width / height;
        float currentWidth = right_ - left_;
        float currentHeight = top_ - bottom_;
        float currentAspect = currentWidth / currentHeight;

        if (newAspect > currentAspect)
        {
            // 宽度需要增加
            float newWidth = currentHeight * newAspect;
            float center = (left_ + right_) / 2.0f;
            left_ = center - newWidth / 2.0f;
            right_ = center + newWidth / 2.0f;
        }
        else
        {
            // 高度需要增加
            float newHeight = currentWidth / newAspect;
            float center = (bottom_ + top_) / 2.0f;
            bottom_ = center - newHeight / 2.0f;
            top_ = center + newHeight / 2.0f;
        }
        updateAspect();
    }

    std::string getType() const override
    {
        return "Orthographic";
    }

    // UI特定的正交投影（像素坐标）
    static OrthographicProjection createForUI(float width, float height)
    {
        return OrthographicProjection(0.0f, width, height, 0.0f, 0.0f, 1.0f);
    }

    // 屏幕空间正交投影（NDC到像素）
    static OrthographicProjection createScreenSpace(float width, float height)
    {
        return OrthographicProjection(0.0f, width, 0.0f, height, -1.0f, 1.0f);
    }

  private:
    void updateAspect()
    {
        aspect_ = (right_ - left_) / (top_ - bottom_);
    }
};

// ==================== 物体变换 ====================
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

// ==================== 相机系统（只负责视图矩阵） ====================
class QuaternionCamera
{
  public:
    glm::vec3 position = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    QuaternionCamera(const glm::vec3 &target = glm::vec3(0, 0, 0),
                     const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        lookAt(target, up);
    }

    glm::mat4 getViewMatrix() const
    {
        glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -position);
        return rotationMatrix * translationMatrix;
    }

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
};

// ==================== 顶点定义 ====================
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

// ==================== 渲染对象 ====================
class RenderObject
{
  public:
    Transform transform;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool enableDepthTest = true;
    std::shared_ptr<Projection> customProjection = nullptr;

    RenderObject(std::vector<Vertex> verts, std::vector<uint32_t> idxs,
                 bool depthTest = true)
        : vertices(std::move(verts)), indices(std::move(idxs)), enableDepthTest(depthTest)
    {
    }

    glm::mat4 calculateMVP(
        const QuaternionCamera &camera,
        const std::shared_ptr<Projection> &sceneProjection = nullptr) const
    {
        glm::mat4 model = transform.getModelMatrix();
        glm::mat4 view = camera.getViewMatrix();

        // 使用自定义投影或场景投影
        std::shared_ptr<Projection> actualProj =
            customProjection ? customProjection : sceneProjection;

        if (actualProj)
        {
            return actualProj->getMatrix() * view * model;
        }
        else
        {
            // 默认透视投影
            PerspectiveProjection defaultProj(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
            return defaultProj.getMatrix() * view * model;
        }
    }

    void setCustomProjection(std::shared_ptr<Projection> proj)
    {
        customProjection = proj;
    }
};

// ==================== 场景管理器 ====================
class Scene
{
  private:
    std::vector<RenderObject> objects;
    QuaternionCamera camera;
    std::shared_ptr<Projection> defaultProjection;

  public:
    Scene()
    {
        defaultProjection = std::make_shared<PerspectiveProjection>();
    }

    void addObject(RenderObject &&obj)
    {
        objects.push_back(std::move(obj));
    }

    void add3DObject(std::vector<Vertex> vertices, std::vector<uint32_t> indices,
                     const Transform &transform = {})
    {
        RenderObject obj(std::move(vertices), std::move(indices), true);
        obj.transform = transform;
        objects.push_back(std::move(obj));
    }

    void addUIObject(std::vector<Vertex> vertices, std::vector<uint32_t> indices,
                     const Transform &transform = {},
                     std::shared_ptr<Projection> uiProjection = nullptr)
    {
        RenderObject obj(std::move(vertices), std::move(indices), false);
        obj.transform = transform;
        if (uiProjection)
        {
            obj.setCustomProjection(uiProjection);
        }
        objects.push_back(std::move(obj));
    }

    const std::vector<RenderObject> &getObjects() const
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

    void setDefaultProjection(std::shared_ptr<Projection> proj)
    {
        defaultProjection = proj;
    }

    std::shared_ptr<Projection> getDefaultProjection() const
    {
        return defaultProjection;
    }
};

// ==================== Uniform Buffer Object ====================
struct UniformBufferObject
{
    alignas(16) glm::mat4 mvp;
};

// ==================== 深度测试配置 ====================
class DepthTesting
{
  public:
    struct Config
    {
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkImage depthImage = VK_NULL_HANDLE;
        VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;
        VkExtent2D extent;

        bool depthTestEnable = VK_TRUE;
        bool depthWriteEnable = VK_TRUE;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        float minDepthBounds = 0.0f;
        float maxDepthBounds = 1.0f;
    };

    static Config createDepthResources(physical_device &physicalDevice,
                                       logical_device &device,
                                       const VkExtent2D &swapChainExtent)
    {
        Config config;
        config.extent = swapChainExtent;

        // 查找深度格式
        config.depthFormat = findDepthFormat(physicalDevice);

        // 创建深度图像
        VkImageCreateInfo imageInfo = {
            .sType = sType<VkImageCreateInfo>(),
            .imageType = VK_IMAGE_TYPE_2D,
            .format = config.depthFormat,
            .extent = {swapChainExtent.width, swapChainExtent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        config.depthImage = device.createImage(imageInfo);

        // 分配内存
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device.raw_data(), config.depthImage,
                                     &memRequirements);

        VkMemoryAllocateInfo allocInfo = {
            .sType = sType<VkMemoryAllocateInfo>(),
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = physicalDevice.findMemoryType(
                memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

        config.depthImageMemory = device.allocateMemory(allocInfo);
        vkBindImageMemory(device.raw_data(), config.depthImage, config.depthImageMemory,
                          0);

        // 创建深度图像视图
        VkImageViewCreateInfo viewInfo = {
            .sType = sType<VkImageViewCreateInfo>(),
            .image = config.depthImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = config.depthFormat,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};

        config.depthImageView = device.createImageView(viewInfo);

        return config;
    }

    static void cleanupDepthResources(logical_device &device, Config &config)
    {
        if (config.depthImageView != VK_NULL_HANDLE)
        {
            device.destroyImageView(config.depthImageView);
        }
        if (config.depthImage != VK_NULL_HANDLE)
        {
            device.destroyImage(config.depthImage);
        }
        if (config.depthImageMemory != VK_NULL_HANDLE)
        {
            device.freeMemory(config.depthImageMemory);
        }
        config = {};
    }

    static VkPipelineDepthStencilStateCreateInfo getDepthStencilState(
        bool enableDepthTest = true)
    {
        return {.sType = sType<VkPipelineDepthStencilStateCreateInfo>(),
                .depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE,
                .depthWriteEnable = enableDepthTest ? VK_TRUE : VK_FALSE,
                .depthCompareOp =
                    enableDepthTest ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS,
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable = VK_FALSE,
                .front = {},
                .back = {},
                .minDepthBounds = 0.0f,
                .maxDepthBounds = 1.0f};
    }

  private:
    static VkFormat findDepthFormat(physical_device &physicalDevice)
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
};

// ==================== 渲染辅助函数 ====================
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

// ==================== 帧同步上下文 ====================
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
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                  &presentCompleteSemaphore[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                  &renderFinishedSemaphore[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphores!");
        }

        VkFenceCreateInfo fenceInfo = {.sType = sType<VkFenceCreateInfo>(),
                                       .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create fence!");
        }
    }

    void destroySyncObject() noexcept
    {
        if (device_)
        {
            for (auto *semaphore : presentCompleteSemaphore)
                vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
            for (auto *semaphore : renderFinishedSemaphore)
                vkDestroySemaphore(device_->raw_data(), semaphore, nullptr);
            for (auto *fence : inFlightFences)
                vkDestroyFence(device_->raw_data(), fence, nullptr);
        }
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
    std::array<FrameBuffer, frame_context::MAX_FRAMES_IN_FLIGHT> frameBuffers;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::array<VkDescriptorSet, frame_context::MAX_FRAMES_IN_FLIGHT> descriptorSets;

  public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool enableDepthTest;

    MeshData(physical_device &physicalDevice, logical_device &device, VkQueue queue,
             VkCommandPool commandPool, const std::vector<Vertex> &vertices,
             const std::vector<uint32_t> &indices, bool depthTest = true)
        : physicalDevice(physicalDevice), device(device), queue(queue),
          commandPool(commandPool), vertices(vertices), indices(indices),
          enableDepthTest(depthTest)
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
    }

    void bindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                           uint32_t currentFrame) const
    {
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
        poolSize.descriptorCount =
            static_cast<uint32_t>(frame_context::MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(frame_context::MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device.raw_data(), &poolInfo, nullptr,
                                   &descriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(frame_context::MAX_FRAMES_IN_FLIGHT,
                                                   descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount =
            static_cast<uint32_t>(frame_context::MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(device.raw_data(), &allocInfo,
                                     descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < frame_context::MAX_FRAMES_IN_FLIGHT; i++)
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

// ==================== 图形管线管理器 ====================
class PipelineManager
{
  private:
    logical_device &device;
    VkPipelineLayout pipelineLayout;
    std::unordered_map<bool, VkPipeline> pipelines; // key: enableDepthTest

  public:
    PipelineManager(logical_device &device) : device(device), pipelineLayout(nullptr) {}

    ~PipelineManager()
    {
        for (auto &[depthTest, pipeline] : pipelines)
        {
            if (pipeline != VK_NULL_HANDLE)
            {
                device.destroyPipeline(pipeline, nullptr);
            }
        }
        if (pipelineLayout != VK_NULL_HANDLE)
        {
            device.destroyPipelineLayout(pipelineLayout, nullptr);
        }
    }

    void createPipelines(VkFormat swapChainImageFormat, VkFormat depthFormat,
                         VkDescriptorSetLayout descriptorSetLayout,
                         const std::string &vertShaderPath,
                         const std::string &fragShaderPath)
    {
        // 创建着色器模块
        mcs::vulkan::shader_module vertShader(device, vertShaderPath);
        mcs::vulkan::shader_module fragShader(device, fragShaderPath);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
            .sType = sType<VkPipelineShaderStageCreateInfo>(),
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShader.raw_data(),
            .pName = "main"};

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
            .sType = sType<VkPipelineShaderStageCreateInfo>(),
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShader.raw_data(),
            .pName = "main"};

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                          fragShaderStageInfo};

        // 顶点输入
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = sType<VkPipelineVertexInputStateCreateInfo>(),
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount =
                static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data()};

        // 输入装配
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType = sType<VkPipelineInputAssemblyStateCreateInfo>(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE};

        // 视口状态
        VkPipelineViewportStateCreateInfo viewportState = {
            .sType = sType<VkPipelineViewportStateCreateInfo>(),
            .viewportCount = 1,
            .scissorCount = 1};

        // 光栅化
        VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = sType<VkPipelineRasterizationStateCreateInfo>(),
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0F};

        // 多重采样
        VkPipelineMultisampleStateCreateInfo multisampling = {
            .sType = sType<VkPipelineMultisampleStateCreateInfo>(),
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE};

        // 颜色混合
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

        // 动态状态
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState = {
            .sType = sType<VkPipelineDynamicStateCreateInfo>(),
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()};

        // 管线布局
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = sType<VkPipelineLayoutCreateInfo>(),
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout};

        pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo, nullptr);

        // 创建两个管线：启用深度测试和禁用深度测试
        // 重要：两个管线都必须使用相同的深度附件格式，否则会有验证层错误
        std::vector<bool> depthTestStates = {true, false};

        for (bool enableDepthTest : depthTestStates)
        {
            VkPipelineDepthStencilStateCreateInfo depthStencil =
                DepthTesting::getDepthStencilState(enableDepthTest);

            // 关键修复：两个管线都使用相同的深度附件格式
            // 即使禁用深度测试，也需要指定正确的深度格式
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
                     .layout = pipelineLayout,
                     .renderPass = VK_NULL_HANDLE},
                    {.colorAttachmentCount = 1,
                     .pColorAttachmentFormats = &swapChainImageFormat,
                     .depthAttachmentFormat = depthFormat}}; // 两个管线都使用 depthFormat

            VkPipeline pipeline = device.createGraphicsPipelines(
                nullptr, 1, pipelineCreateInfoChain.head(), nullptr);

            pipelines[enableDepthTest] = pipeline;
        }
    }

    VkPipeline getPipeline(bool enableDepthTest) const
    {
        auto it = pipelines.find(enableDepthTest);
        if (it != pipelines.end())
        {
            return it->second;
        }
        return VK_NULL_HANDLE;
    }

    VkPipelineLayout getPipelineLayout() const
    {
        return pipelineLayout;
    }
};

// ==================== 常量定义 ====================
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr auto TITLE = "MVP Projection Test";
constexpr auto VERT_SHADER_PATH = "shaders/test_model_vert.spv";
constexpr auto FRAG_SHADER_PATH = "shaders/test_model_frag.spv";

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

// ==================== 主函数 ====================
int main()
{
    try
    {
        // 1. 初始化窗口
        surface window{};
        window.setup({.width = WIDTH, .height = HEIGHT}, TITLE);

        // 2. 创建Vulkan实例
        std::vector<const char *> requiredDeviceExtension = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

        // 重要：启用 dynamicRenderingUnusedAttachments 特性
        structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT,
                        VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT>
            enablefeatureChain = {
                {},
                {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                {.extendedDynamicState = VK_TRUE},
                {.dynamicRenderingUnusedAttachments = VK_TRUE}}; // 启用这个特性

        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "MVP Projection Test",
                                    .applicationVersion = VkApiVersion(1, 0, 0),
                                    .pEngineName = "No Engine",
                                    .engineVersion = VkApiVersion(1, 0, 0),
                                    .apiVersion = VkApiVersion(0, 1, 3, 0)});

        VkSurfaceKHR surface_ = window.createVkSurfaceKHR(instance.ref_data());
        assert(surface_ != nullptr);

        // 3. 选择物理设备
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
                .requiredFeatures([](const physical_device
                                         &physicalDevice) constexpr noexcept -> bool {
                    auto query = structure_chain<
                        VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT,
                        VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT>{
                        {}, {}, {}, {}};
                    physicalDevice.getFeatures2(query.head());
                    auto &query_vulkan13_features =
                        query.template get<VkPhysicalDeviceVulkan13Features>();
                    auto &query_extended_dynamic_state_features = query.template get<
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                    // 检查 dynamicRenderingUnusedAttachments 是否可用
                    auto &UnusedAttachments = query.template get<
                        VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT>();

                    return query_vulkan13_features.dynamicRendering &&
                           query_vulkan13_features.synchronization2 &&
                           query_extended_dynamic_state_features.extendedDynamicState &&
                           UnusedAttachments.dynamicRenderingUnusedAttachments;
                })
                .pickPhysicalDevice();

        // 4. 创建设备
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

        // 5. 创建交换链
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

        // 6. 创建交换链图像视图
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

        // 7. 创建深度测试资源
        DepthTesting::Config depthConfig = DepthTesting::createDepthResources(
            physicalDevice, logical_device_, swapChainExtent);

        // 8. 创建命令池
        auto *commandPool = logical_device_.createCommandPool(
            {.sType = sType<VkCommandPoolCreateInfo>(),
             .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
             .queueFamilyIndex = graphicsIndex});

        // 9. 创建命令缓冲区
        std::array<VkCommandBuffer, frame_context::MAX_FRAMES_IN_FLIGHT> commandBuffers{};
        logical_device_.allocateCommandBuffers(
            commandBuffers[0],
            {.sType = sType<VkCommandBufferAllocateInfo>(),
             .commandPool = commandPool,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())});

        // 10. 创建帧同步上下文
        frame_context frameContext{logical_device_, swapChainImages.size()};

        // ==================== 创建场景和对象 ====================
        Scene scene;
        // 修复相机位置
        scene.getCamera().position = glm::vec3(0.0f, 0.0f, -10.0f); // 向后移动更多
        scene.getCamera().lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        // 创建不同的投影
        auto perspectiveProj = std::make_shared<PerspectiveProjection>(
            60.0f, static_cast<float>(WIDTH) / HEIGHT, 0.1f, 100.0f);
        scene.setDefaultProjection(perspectiveProj);

        // 修复：创建正确的UI正交投影（像素坐标，Y向下）
        auto orthoProjUI = std::make_shared<OrthographicProjection>(
            0.0f, static_cast<float>(WIDTH),  // left, right
            static_cast<float>(HEIGHT), 0.0f, // bottom, top (Y向下)
            0.0f, 1.0f                        // near, far
        );

        // ==================== 3D对象：立方体 ====================
        std::vector<Vertex> cubeVertices = {
            // 前面（红色） - 增大尺寸以便看清楚
            {{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

            // 后面（绿色）
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        };

        std::vector<uint32_t> cubeIndices = {
            0, 1, 2, 2, 3, 0, // 前
            4, 5, 6, 6, 7, 4, // 后
            0, 3, 7, 7, 4, 0, // 左
            1, 2, 6, 6, 5, 1, // 右
            3, 2, 6, 6, 7, 3, // 上
            0, 1, 5, 5, 4, 0  // 下
        };

        Transform cubeTransform;
        cubeTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        cubeTransform.scale = glm::vec3(0.5f, 0.5f, 0.5f); // 缩小一点

        // ==================== UI对象：屏幕左上角的矩形 ====================
        // 使用像素坐标：左上角 (50, 50) 到 (150, 150)
        std::vector<Vertex> uiRect1 = {
            {{50.0f, 150.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // 左下
            {{150.0f, 150.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // 右下
            {{150.0f, 50.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},  // 右上
            {{50.0f, 50.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}    // 左上
        };

        std::vector<uint32_t> uiIndices = {0, 1, 2, 2, 3, 0};

        Transform uiTransform1;
        uiTransform1.position = glm::vec3(0.0f, 0.0f, 0.0f);

        scene.addUIObject(uiRect1, uiIndices, uiTransform1, orthoProjUI);

        // ==================== UI对象：屏幕右上角的矩形 ====================
        std::vector<Vertex> uiRect2 = {
            {{static_cast<float>(WIDTH - 150), 150.0f, 0.0f}, {1.0f, 0.5f, 0.0f}}, // 左下
            {{static_cast<float>(WIDTH - 50), 150.0f, 0.0f}, {0.5f, 1.0f, 0.0f}},  // 右下
            {{static_cast<float>(WIDTH - 50), 50.0f, 0.0f}, {0.0f, 0.5f, 1.0f}},   // 右上
            {{static_cast<float>(WIDTH - 150), 50.0f, 0.0f}, {1.0f, 0.0f, 0.5f}}   // 左上
        };

        Transform uiTransform2;
        uiTransform2.position = glm::vec3(0.0f, 0.0f, 0.0f);

        scene.addUIObject(uiRect2, uiIndices, uiTransform2, orthoProjUI);

        // 11. 为场景中的每个对象创建MeshData
        std::vector<std::unique_ptr<MeshData>> meshDatas;
        for (const auto &obj : scene.getObjects())
        {
            meshDatas.push_back(std::make_unique<MeshData>(
                physicalDevice, logical_device_, graphicsQueue, commandPool, obj.vertices,
                obj.indices, obj.enableDepthTest));
        }

        // 12. 创建管线管理器
        PipelineManager pipelineManager(logical_device_);
        pipelineManager.createPipelines(swapChainImageFormat, depthConfig.depthFormat,
                                        meshDatas[0]->getDescriptorSetLayout(),
                                        VERT_SHADER_PATH, FRAG_SHADER_PATH);

        // 13. 清理交换链函数
        auto cleanupSwapChain = [&]() {
            for (auto *imageView : swapChainImageViews)
                logical_device_.destroyImageView(imageView, nullptr);
            if (swapChain != nullptr)
                logical_device_.destroySwapchainKHR(swapChain);

            swapChainImageViews.clear();
            swapChain = nullptr;
        };

        // 14. 重新创建交换链函数
        auto recreateSwapChain = [&]() {
            window.waitGoodFramebufferSize();
            logical_device_.waitIdle();

            cleanupSwapChain();
            DepthTesting::cleanupDepthResources(logical_device_, depthConfig);

            createSwapChain();
            createImageViews();
            depthConfig = DepthTesting::createDepthResources(
                physicalDevice, logical_device_, swapChainExtent);

            // 更新投影的宽高比
            auto proj = std::dynamic_pointer_cast<PerspectiveProjection>(
                scene.getDefaultProjection());
            if (proj)
            {
                proj->updateAspectRatio(static_cast<float>(swapChainExtent.width),
                                        static_cast<float>(swapChainExtent.height));
            }

            // 更新UI投影
            auto uiProj = std::dynamic_pointer_cast<OrthographicProjection>(orthoProjUI);
            if (uiProj)
            {
                uiProj->updateAspectRatio(static_cast<float>(swapChainExtent.width),
                                          static_cast<float>(swapChainExtent.height));
            }
        };

        // 15. 记录命令缓冲区
        auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer,
                                       uint32_t currentFrame, uint32_t imageIndex) {
            VkCommandBufferBeginInfo beginInfo = {.sType =
                                                      sType<VkCommandBufferBeginInfo>()};
            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkImage image = swapChainImages[imageIndex];
            VkImageView imageView = swapChainImageViews[imageIndex];

            // 转换颜色图像布局
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            // 转换深度图像布局
            if (depthConfig.depthImage != VK_NULL_HANDLE)
            {
                my_render::transition_image_layout(
                    commandBuffer, depthConfig.depthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    {}, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
            }

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
                .imageView = depthConfig.depthImageView,
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

            // 获取相机和对象列表
            auto &camera = scene.getCamera();
            const auto &objects = scene.getObjects();

            // ==================== MVP测试区域 ====================
            // 这里可以添加您的MVP测试代码
            // 例如：
            // 1. 测试不同的相机位置
            // camera.position = glm::vec3(0.0f, 0.0f, -5.0f);
            // camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));

            // 2. 测试不同的投影
            // auto newProj = std::make_shared<PerspectiveProjection>(90.0f, ...);
            // scene.setDefaultProjection(newProj);

            // 3. 测试物体变换
            // if (!objects.empty()) {
            //     auto& obj = const_cast<RenderObject&>(objects[0]);
            //     obj.transform.position.x += 0.01f;
            // }

            // 4. 动态旋转立方体（取消注释以下代码）
            // static float rotationAngle = 0.0f;
            // rotationAngle += 0.5f;
            // if (rotationAngle > 360.0f)
            //     rotationAngle -= 360.0f;
            // auto &cubeObj = const_cast<RenderObject &>(objects[0]);
            // cubeObj.transform.setRotation(
            //     glm::angleAxis(glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f,
            //     0.0f)));
            //

            // NOTE: 开启旋转,也是什么都看不到
            //  测试A：随时间旋转相机
            static float rotationAngle = 0.0f;
            rotationAngle += 0.5f; // 每帧旋转0.5度
            if (rotationAngle > 360.0f)
                rotationAngle -= 360.0f;
            camera.rotateYaw(0.5f); // 每帧水平旋转0.5度
            // ====================================================

            // 渲染所有对象
            for (size_t i = 0; i < objects.size(); i++)
            {
                if (i < meshDatas.size())
                {
                    // 计算MVP矩阵
                    glm::mat4 mvp = objects[i].calculateMVP(
                        camera, objects[i].customProjection
                                    ? objects[i].customProjection
                                    : scene.getDefaultProjection());

                    // 更新Uniform Buffer
                    meshDatas[i]->updateUniformBuffer(currentFrame, mvp);

                    // 绑定管线（根据是否启用深度测试）
                    VkPipeline pipeline =
                        pipelineManager.getPipeline(objects[i].enableDepthTest);
                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      pipeline);

                    // 设置视口和裁剪
                    VkViewport viewport = {
                        .x = 0.0F,
                        .y = 0.0F,
                        .width = static_cast<float>(swapChainExtent.width),
                        .height = static_cast<float>(swapChainExtent.height),
                        .minDepth = 0.0F,
                        .maxDepth = 1.0F};
                    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

                    VkRect2D scissor = {.offset = {.x = 0, .y = 0},
                                        .extent = swapChainExtent};
                    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                    // 绑定顶点缓冲区和描述符集
                    meshDatas[i]->bind(commandBuffer, currentFrame);
                    meshDatas[i]->bindDescriptorSet(
                        commandBuffer, pipelineManager.getPipelineLayout(), currentFrame);

                    // 绘制
                    meshDatas[i]->draw(commandBuffer);
                }
            }

            vkCmdEndRendering(commandBuffer);

            // 转换颜色图像布局到呈现
            my_render::transition_image_layout(
                commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        };

        // 16. 绘制帧
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
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
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

            result = vkQueuePresentKHR(presentQueue, &presentInfo);

            if (auto &framebufferResized = window.refFramebufferResized();
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                framebufferResized)
            {
                framebufferResized = false;
                recreateSwapChain();
            }

            semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
            currentFrame = (currentFrame + 1) % frame_context::MAX_FRAMES_IN_FLIGHT;
        };

        // 17. 主循环
        std::cout << "=== MVP Projection Test Started ===" << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "  1. 3D Cube with Perspective Projection (depth test enabled)"
                  << std::endl;
        std::cout << "  2. UI rectangles with Orthographic Projection (no depth test)"
                  << std::endl;
        std::cout << "  3. Separate projection system" << std::endl;
        std::cout << "  4. Depth testing for proper 3D rendering" << std::endl;
        std::cout << "==================================" << std::endl;
        std::cout << std::endl;

        // 打印初始MVP信息
        std::cout << "Initial MVP Configuration:" << std::endl;
        std::cout << "  Camera position: (" << scene.getCamera().position.x << ", "
                  << scene.getCamera().position.y << ", " << scene.getCamera().position.z
                  << ")" << std::endl;
        std::cout << "  Projection type: " << scene.getDefaultProjection()->getType()
                  << std::endl;

        // 示例：打印立方体的MVP矩阵（第一个对象）
        if (!scene.getObjects().empty())
        {
            const auto &firstObj = scene.getObjects()[0];
            glm::mat4 mvp =
                firstObj.calculateMVP(scene.getCamera(), scene.getDefaultProjection());
            std::cout << "  Cube MVP matrix determinant: " << glm::determinant(mvp)
                      << std::endl;
        }

        std::cout << std::endl;
        std::cout << "Window is running. Close window to exit." << std::endl;
        std::cout << "==================================" << std::endl;

        while (window.shouldClose() == 0)
        {
            surface::pollEvents();
            drawFrame();
        }

        // 18. 清理
        vkDeviceWaitIdle(logical_device_.raw_data());

        // 清理深度资源
        DepthTesting::cleanupDepthResources(logical_device_, depthConfig);

        cleanupSwapChain();
        logical_device_.destroyCommandPool(commandPool);
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface_);

        std::cout << "\n=== MVP Projection Test Finished ===" << std::endl;
    }
    catch (std::exception &e)
    {
        std::println("Error: {}", e.what());
    }

    return 0;
}