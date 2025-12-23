#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <vulkan/vulkan.h>

#include <array>

#include <chrono>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord; // NOTE: 5. 增加顶点的 纹理坐标，“uv坐标”。// NOLINT

    static_assert(sizeof(float) == 4);

    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return {
            VkVertexInputAttributeDescription{.location = 0,
                                              .binding = 0,
                                              .format =
                                                  VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
                                              .offset = offsetof(Vertex, pos)},
            VkVertexInputAttributeDescription{.location = 1,
                                              .binding = 0,
                                              .format =
                                                  VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
                                              .offset = offsetof(Vertex, color)},
            VkVertexInputAttributeDescription{.location = 2,
                                              .binding = 0,
                                              .format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
                                              .offset = offsetof(Vertex, texCoord)}};
    }

    bool operator==(const Vertex &other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};
template <>
struct std::hash<Vertex>
{
    size_t operator()(Vertex const &vertex) const noexcept
    {
        return ((hash<glm::vec3>()(vertex.pos) ^
                 (hash<glm::vec3>()(vertex.color) << 1)) >>
                1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};