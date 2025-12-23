#pragma once

#include "head.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

// 简化的顶点结构 - 全屏四边形
struct shadertoy_vertex
{
    glm::vec2 position;
    glm::vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0,
                .stride = sizeof(shadertoy_vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {
            VkVertexInputAttributeDescription{.location = 0,
                                              .binding = 0,
                                              .format = VK_FORMAT_R32G32_SFLOAT,
                                              .offset =
                                                  offsetof(shadertoy_vertex, position)},
            VkVertexInputAttributeDescription{.location = 1,
                                              .binding = 0,
                                              .format = VK_FORMAT_R32G32_SFLOAT,
                                              .offset = offsetof(shadertoy_vertex, uv)}};
    }
};