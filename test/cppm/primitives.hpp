#pragma once

// primitives.hpp - 完整的图元生成器

#include <vector>
#include <cmath>
#include <numbers>

#include "./vertex.hpp"

// NOLINTBEGIN
namespace primitives
{

    // 图元类型枚举
    enum class PrimitiveType
    {
        SPHERE,              // 球体
        CUBE,                // 立方体
        PLANE,               // 平面
        CYLINDER,            // 圆柱体
        CONE,                // 圆锥体
        TORUS,               // 圆环体
        GRID,                // 网格
        LINE_CUBE,           // 线框立方体
        AXES,                // 坐标轴
        SIN_WAVE,            // 正弦波
        POINTS,              // 点云
        TRIANGLE_STRIP_QUAD, // 三角形带四边形
        TRIANGLE_FAN_CIRCLE  // 三角形扇形圆
    };

    // 图元生成参数
    struct PrimitiveParams
    {
        PrimitiveType type = PrimitiveType::SPHERE;
        glm::vec3 color = {1.0f, 1.0f, 1.0f};
        glm::vec2 uvScale = {1.0f, 1.0f};
        float param1 = 0.5f;   // 半径/宽度等
        float param2 = 0.5f;   // 高度/深度等
        float param3 = 1.0f;   // 额外参数
        int subdivisions = 16; // 细分等级
        int pointCount = 100;  // 仅用于点云
    };

    class PrimitiveGenerator
    {
      public:
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>> generate(
            const PrimitiveParams &params)
        {
            switch (params.type)
            {
            case PrimitiveType::SPHERE:
                return generateSphere(params);
            case PrimitiveType::CUBE:
                return generateCube(params);
            case PrimitiveType::PLANE:
                return generatePlane(params);
            case PrimitiveType::CYLINDER:
                return generateCylinder(params);
            case PrimitiveType::CONE:
                return generateCone(params);
            case PrimitiveType::TORUS:
                return generateTorus(params);
            case PrimitiveType::GRID:
                return generateGridAsTriangles(params);
            case PrimitiveType::LINE_CUBE:
                return generateLineCubeAsTriangles(params);
            case PrimitiveType::AXES:
                return generateAxesAsTriangles(params);
            case PrimitiveType::SIN_WAVE:
                return generateSinWaveAsTriangles(params);
            case PrimitiveType::POINTS:
                return generatePointsAsTriangles(params);
            case PrimitiveType::TRIANGLE_STRIP_QUAD:
                return generateTriangleStripQuadAsTriangles(params);
            case PrimitiveType::TRIANGLE_FAN_CIRCLE:
                return generateTriangleFanCircleAsTriangles(params);
            default:
                return generateSphere(params);
            }
        }

      private:
        // ==================== 球体 ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>> generateSphere(
            const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float radius = params.param1;
            int stacks = params.subdivisions;
            int sectors = params.subdivisions;

            for (int i = 0; i <= stacks; ++i)
            {
                float stackAngle = static_cast<float>(i) / static_cast<float>(stacks) *
                                   std::numbers::pi_v<float>;
                float sinStack = sin(stackAngle);
                float cosStack = cos(stackAngle);

                for (int j = 0; j <= sectors; ++j)
                {
                    float sectorAngle = static_cast<float>(j) /
                                        static_cast<float>(sectors) * 2.0f *
                                        std::numbers::pi_v<float>;
                    float sinSector = sin(sectorAngle);
                    float cosSector = cos(sectorAngle);

                    Vertex vertex{};
                    vertex.pos.x = radius * sinStack * cosSector;
                    vertex.pos.y = radius * cosStack;
                    vertex.pos.z = radius * sinStack * sinSector;
                    vertex.color = params.color;
                    vertex.texCoord.x = static_cast<float>(j) /
                                        static_cast<float>(sectors) * params.uvScale.x;
                    vertex.texCoord.y = 1.0f - static_cast<float>(i) /
                                                   static_cast<float>(stacks) *
                                                   params.uvScale.y;

                    vertices.push_back(vertex);
                }
            }

            for (int i = 0; i < stacks; ++i)
            {
                for (int j = 0; j < sectors; ++j)
                {
                    int first = (i * (sectors + 1)) + j;
                    int second = first + sectors + 1;

                    indices.push_back(first);
                    indices.push_back(second);
                    indices.push_back(first + 1);

                    indices.push_back(first + 1);
                    indices.push_back(second);
                    indices.push_back(second + 1);
                }
            }

            return {vertices, indices};
        }

        // ==================== 立方体 ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>> generateCube(
            const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float halfSize = params.param1 * 0.5f;

            // 8个顶点
            std::vector<glm::vec3> positions = {
                {-halfSize, -halfSize, halfSize},  // 0: 前左下
                {halfSize, -halfSize, halfSize},   // 1: 前右下
                {halfSize, halfSize, halfSize},    // 2: 前右上
                {-halfSize, halfSize, halfSize},   // 3: 前左上
                {-halfSize, -halfSize, -halfSize}, // 4: 后左下
                {halfSize, -halfSize, -halfSize},  // 5: 后右下
                {halfSize, halfSize, -halfSize},   // 6: 后右上
                {-halfSize, halfSize, -halfSize}   // 7: 后左上
            };

            // 6个面的颜色
            std::vector<glm::vec3> colors = {
                params.color * glm::vec3(1.0f, 0.0f, 0.0f), // 前面 (红色)
                params.color * glm::vec3(0.0f, 1.0f, 0.0f), // 后面 (绿色)
                params.color * glm::vec3(0.0f, 0.0f, 1.0f), // 上面 (蓝色)
                params.color * glm::vec3(1.0f, 1.0f, 0.0f), // 下面 (黄色)
                params.color * glm::vec3(1.0f, 0.0f, 1.0f), // 右面 (紫色)
                params.color * glm::vec3(0.0f, 1.0f, 1.0f)  // 左面 (青色)
            };

            // 6个面的纹理坐标和顶点索引
            struct Face
            {
                std::vector<int> vertexIndices;
                glm::vec2 uv0, uv1, uv2, uv3;
                glm::vec3 color;
            };

            std::vector<Face> faces = {
                {{0, 1, 2, 3}, {0, 0}, {1, 0}, {1, 1}, {0, 1}, colors[0]}, // 前面
                {{5, 4, 7, 6}, {0, 0}, {1, 0}, {1, 1}, {0, 1}, colors[1]}, // 后面
                {{3, 2, 6, 7}, {0, 0}, {1, 0}, {1, 1}, {0, 1}, colors[2]}, // 上面
                {{4, 5, 1, 0}, {0, 0}, {1, 0}, {1, 1}, {0, 1}, colors[3]}, // 下面
                {{1, 5, 6, 2}, {0, 0}, {1, 0}, {1, 1}, {0, 1}, colors[4]}, // 右面
                {{4, 0, 3, 7}, {0, 0}, {1, 0}, {1, 1}, {0, 1}, colors[5]}  // 左面
            };

            // 生成顶点和索引
            for (const auto &face : faces)
            {
                uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

                vertices.push_back(
                    {positions[face.vertexIndices[0]], face.color, face.uv0});
                vertices.push_back(
                    {positions[face.vertexIndices[1]], face.color, face.uv1});
                vertices.push_back(
                    {positions[face.vertexIndices[2]], face.color, face.uv2});
                vertices.push_back(
                    {positions[face.vertexIndices[3]], face.color, face.uv3});

                indices.push_back(baseIndex);
                indices.push_back(baseIndex + 1);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 3);
                indices.push_back(baseIndex);
            }

            return {vertices, indices};
        }

        // ==================== 平面 ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>> generatePlane(
            const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float width = params.param1;
            float depth = params.param2;

            float halfWidth = width * 0.5f;
            float halfDepth = depth * 0.5f;

            vertices = {
                {{-halfWidth, 0.0f, -halfDepth}, params.color, {0.0f, 0.0f}},
                {{halfWidth, 0.0f, -halfDepth}, params.color, {params.uvScale.x, 0.0f}},
                {{halfWidth, 0.0f, halfDepth},
                 params.color,
                 {params.uvScale.x, params.uvScale.y}},
                {{-halfWidth, 0.0f, halfDepth}, params.color, {0.0f, params.uvScale.y}}};

            indices = {0, 1, 2, 2, 3, 0};

            return {vertices, indices};
        }

        // ==================== 圆柱体 ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>> generateCylinder(
            const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float radius = params.param1;
            float height = params.param2;
            int sectors = params.subdivisions;

            float halfHeight = height * 0.5f;

            // 侧面顶点
            for (int i = 0; i <= sectors; ++i)
            {
                float angle = static_cast<float>(i) / static_cast<float>(sectors) * 2.0f *
                              std::numbers::pi_v<float>;
                float sinAngle = sin(angle);
                float cosAngle = cos(angle);

                // 底部圆环
                Vertex bottomVertex{};
                bottomVertex.pos = {radius * cosAngle, -halfHeight, radius * sinAngle};
                bottomVertex.color = params.color;
                bottomVertex.texCoord = {static_cast<float>(i) / sectors, 0.0f};
                vertices.push_back(bottomVertex);

                // 顶部圆环
                Vertex topVertex{};
                topVertex.pos = {radius * cosAngle, halfHeight, radius * sinAngle};
                topVertex.color = params.color;
                topVertex.texCoord = {static_cast<float>(i) / sectors, 1.0f};
                vertices.push_back(topVertex);
            }

            // 侧面索引
            for (int i = 0; i < sectors; ++i)
            {
                int base = i * 2;
                indices.push_back(base);
                indices.push_back(base + 1);
                indices.push_back(base + 2);

                indices.push_back(base + 2);
                indices.push_back(base + 1);
                indices.push_back(base + 3);
            }

            // 顶部和底部圆面
            int topCenterIndex = static_cast<int>(vertices.size());
            vertices.push_back({{0.0f, halfHeight, 0.0f}, params.color, {0.5f, 0.5f}});

            int bottomCenterIndex = static_cast<int>(vertices.size());
            vertices.push_back({{0.0f, -halfHeight, 0.0f}, params.color, {0.5f, 0.5f}});

            for (int i = 0; i < sectors; ++i)
            {
                int next = (i + 1) % sectors;
                int top1 = i * 2 + 1;
                int top2 = next * 2 + 1;
                int bottom1 = i * 2;
                int bottom2 = next * 2;

                // 顶部圆面
                indices.push_back(topCenterIndex);
                indices.push_back(top1);
                indices.push_back(top2);

                // 底部圆面（注意顶点顺序）
                indices.push_back(bottomCenterIndex);
                indices.push_back(bottom2);
                indices.push_back(bottom1);
            }

            return {vertices, indices};
        }

        // ==================== 圆锥体 ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>> generateCone(
            const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float radius = params.param1;
            float height = params.param2;
            int sectors = params.subdivisions;

            float halfHeight = height * 0.5f;

            // 顶点：顶部尖点
            vertices.push_back({{0.0f, halfHeight, 0.0f}, params.color, {0.5f, 1.0f}});

            // 底部圆环
            for (int i = 0; i <= sectors; ++i)
            {
                float angle = static_cast<float>(i) / static_cast<float>(sectors) * 2.0f *
                              std::numbers::pi_v<float>;
                float sinAngle = sin(angle);
                float cosAngle = cos(angle);

                Vertex vertex{};
                vertex.pos = {radius * cosAngle, -halfHeight, radius * sinAngle};
                vertex.color = params.color;
                vertex.texCoord = {static_cast<float>(i) / sectors, 0.0f};
                vertices.push_back(vertex);
            }

            // 侧面索引
            for (int i = 1; i <= sectors; ++i)
            {
                indices.push_back(0); // 顶部尖点
                indices.push_back(i);
                indices.push_back(i + 1);
            }

            // 底部圆面
            int bottomCenterIndex = static_cast<int>(vertices.size());
            vertices.push_back({{0.0f, -halfHeight, 0.0f}, params.color, {0.5f, 0.5f}});

            for (int i = 1; i <= sectors; ++i)
            {
                int next = (i % sectors) + 1;
                indices.push_back(bottomCenterIndex);
                indices.push_back(next);
                indices.push_back(i);
            }

            return {vertices, indices};
        }

        // ==================== 圆环体 ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>> generateTorus(
            const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float majorRadius = params.param1; // 主半径
            float minorRadius = params.param2; // 小半径
            int majorSegments = params.subdivisions;
            int minorSegments = params.subdivisions;

            for (int i = 0; i <= majorSegments; ++i)
            {
                float u = static_cast<float>(i) / static_cast<float>(majorSegments) *
                          2.0f * std::numbers::pi_v<float>;
                float cosU = cos(u);
                float sinU = sin(u);

                for (int j = 0; j <= minorSegments; ++j)
                {
                    float v = static_cast<float>(j) / static_cast<float>(minorSegments) *
                              2.0f * std::numbers::pi_v<float>;
                    float cosV = cos(v);
                    float sinV = sin(v);

                    Vertex vertex{};
                    vertex.pos.x = (majorRadius + minorRadius * cosV) * cosU;
                    vertex.pos.y = minorRadius * sinV;
                    vertex.pos.z = (majorRadius + minorRadius * cosV) * sinU;
                    vertex.color = params.color;
                    vertex.texCoord.x =
                        static_cast<float>(i) / majorSegments * params.uvScale.x;
                    vertex.texCoord.y =
                        static_cast<float>(j) / minorSegments * params.uvScale.y;

                    vertices.push_back(vertex);
                }
            }

            for (int i = 0; i < majorSegments; ++i)
            {
                for (int j = 0; j < minorSegments; ++j)
                {
                    int first = (i * (minorSegments + 1)) + j;
                    int second = first + minorSegments + 1;

                    indices.push_back(first);
                    indices.push_back(second);
                    indices.push_back(first + 1);

                    indices.push_back(second);
                    indices.push_back(second + 1);
                    indices.push_back(first + 1);
                }
            }

            return {vertices, indices};
        }

        // ==================== 网格（转换为三角形） ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        generateGridAsTriangles(const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float size = params.param1;
            int divisions = params.subdivisions;
            float step = size / divisions;
            float halfSize = size * 0.5f;

            // 创建网格平面作为三角形
            for (int i = 0; i <= divisions; ++i)
            {
                for (int j = 0; j <= divisions; ++j)
                {
                    float x = -halfSize + i * step;
                    float z = -halfSize + j * step;

                    Vertex vertex{};
                    vertex.pos = {x, 0.0f, z};
                    vertex.color = params.color;
                    vertex.texCoord = {
                        static_cast<float>(i) / divisions * params.uvScale.x,
                        static_cast<float>(j) / divisions * params.uvScale.y};
                    vertices.push_back(vertex);
                }
            }

            // 创建三角形索引
            for (int i = 0; i < divisions; ++i)
            {
                for (int j = 0; j < divisions; ++j)
                {
                    int topLeft = i * (divisions + 1) + j;
                    int topRight = topLeft + 1;
                    int bottomLeft = (i + 1) * (divisions + 1) + j;
                    int bottomRight = bottomLeft + 1;

                    // 第一个三角形
                    indices.push_back(topLeft);
                    indices.push_back(bottomLeft);
                    indices.push_back(topRight);

                    // 第二个三角形
                    indices.push_back(topRight);
                    indices.push_back(bottomLeft);
                    indices.push_back(bottomRight);
                }
            }

            return {vertices, indices};
        }

        // ==================== 线框立方体（转换为三角形） ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        generateLineCubeAsTriangles(const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float halfSize = params.param1 * 0.5f;

            // 创建实心立方体（而不是线框）
            return generateCube(params);
        }

        // ==================== 坐标轴（转换为三角形） ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        generateAxesAsTriangles(const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float length = params.param1;
            float thickness = length * 0.05f; // 轴的厚度

            // X轴 (红色) - 创建为细长的长方体
            PrimitiveParams xAxisParams = params;
            xAxisParams.color = {1.0f, 0.0f, 0.0f};
            xAxisParams.param1 = thickness; // 宽度
            xAxisParams.param2 = thickness; // 高度
            xAxisParams.param3 = length;    // 深度

            auto [xVertices, xIndices] = generateCube(xAxisParams);

            // 变换X轴顶点
            for (auto &vertex : xVertices)
            {
                // 旋转使其沿X轴方向
                glm::vec3 pos = vertex.pos;
                vertex.pos = glm::vec3(pos.z, pos.y, pos.x);
                // 平移使其从原点开始
                vertex.pos.x += length * 0.5f;
            }

            // 调整索引
            uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
            vertices.insert(vertices.end(), xVertices.begin(), xVertices.end());
            for (auto index : xIndices)
            {
                indices.push_back(baseIndex + index);
            }

            // Y轴 (绿色) - 创建为细长的长方体
            PrimitiveParams yAxisParams = params;
            yAxisParams.color = {0.0f, 1.0f, 0.0f};
            yAxisParams.param1 = thickness; // 宽度
            yAxisParams.param2 = length;    // 高度
            yAxisParams.param3 = thickness; // 深度

            auto [yVertices, yIndices] = generateCube(yAxisParams);

            // 变换Y轴顶点
            baseIndex = static_cast<uint32_t>(vertices.size());
            vertices.insert(vertices.end(), yVertices.begin(), yVertices.end());
            for (auto index : yIndices)
            {
                indices.push_back(baseIndex + index);
            }

            // Z轴 (蓝色) - 创建为细长的长方体
            PrimitiveParams zAxisParams = params;
            zAxisParams.color = {0.0f, 0.0f, 1.0f};
            zAxisParams.param1 = length;    // 宽度
            zAxisParams.param2 = thickness; // 高度
            zAxisParams.param3 = thickness; // 深度

            auto [zVertices, zIndices] = generateCube(zAxisParams);

            // 变换Z轴顶点
            for (auto &vertex : zVertices)
            {
                // 旋转使其沿Z轴方向
                glm::vec3 pos = vertex.pos;
                vertex.pos = glm::vec3(pos.x, pos.z, pos.y);
                // 平移使其从原点开始
                vertex.pos.z += length * 0.5f;
            }

            baseIndex = static_cast<uint32_t>(vertices.size());
            vertices.insert(vertices.end(), zVertices.begin(), zVertices.end());
            for (auto index : zIndices)
            {
                indices.push_back(baseIndex + index);
            }

            return {vertices, indices};
        }

        // ==================== 正弦波（转换为三角形） ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        generateSinWaveAsTriangles(const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float width = params.param1;
            float amplitude = params.param2;
            int segments = params.subdivisions;
            float thickness = width * 0.02f; // 波的厚度

            // 创建正弦波表面作为三角形带
            for (int i = 0; i <= segments; ++i)
            {
                float x = (static_cast<float>(i) / segments - 0.5f) * width;
                float z = amplitude * sin(x * std::numbers::pi_v<float> * 2.0f);

                // 顶部顶点
                Vertex topVertex{};
                topVertex.pos = {x, thickness * 0.5f, z};
                topVertex.color = params.color;
                topVertex.texCoord = {static_cast<float>(i) / segments, 0.0f};
                vertices.push_back(topVertex);

                // 底部顶点
                Vertex bottomVertex{};
                bottomVertex.pos = {x, -thickness * 0.5f, z};
                bottomVertex.color = params.color;
                bottomVertex.texCoord = {static_cast<float>(i) / segments, 1.0f};
                vertices.push_back(bottomVertex);
            }

            // 创建三角形索引
            for (int i = 0; i < segments; ++i)
            {
                int currentTop = i * 2;
                int currentBottom = currentTop + 1;
                int nextTop = (i + 1) * 2;
                int nextBottom = nextTop + 1;

                // 第一个三角形
                indices.push_back(currentTop);
                indices.push_back(currentBottom);
                indices.push_back(nextTop);

                // 第二个三角形
                indices.push_back(nextTop);
                indices.push_back(currentBottom);
                indices.push_back(nextBottom);
            }

            return {vertices, indices};
        }

        // ==================== 点云（转换为三角形） ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        generatePointsAsTriangles(const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float radius = params.param1;
            int count = params.pointCount;
            float pointSize = radius * 0.02f; // 点的大小

            // 为每个点创建一个小的立方体（用三角形表示）
            uint32_t vertexIndex = 0;
            for (int i = 0; i < count; ++i)
            {
                float u = static_cast<float>(rand()) / RAND_MAX;
                float v = static_cast<float>(rand()) / RAND_MAX;

                float theta = 2.0f * std::numbers::pi_v<float> * u;
                float phi = acos(2.0f * v - 1.0f);

                // 点的中心位置
                glm::vec3 center = {radius * sin(phi) * cos(theta),
                                    radius * sin(phi) * sin(theta), radius * cos(phi)};

                // 创建小的立方体参数
                PrimitiveParams pointParams = params;
                pointParams.param1 = pointSize; // 立方体大小

                auto [pointVertices, pointIndices] = generateCube(pointParams);

                // 平移顶点到正确位置
                for (auto &vertex : pointVertices)
                {
                    vertex.pos += center;
                }

                // 添加顶点和索引
                uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
                vertices.insert(vertices.end(), pointVertices.begin(),
                                pointVertices.end());
                for (auto index : pointIndices)
                {
                    indices.push_back(baseIndex + index);
                }
            }

            return {vertices, indices};
        }

        // ==================== 三角形带四边形（转换为三角形列表） ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        generateTriangleStripQuadAsTriangles(const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float width = params.param1;
            float height = params.param2;

            float halfWidth = width * 0.5f;
            float halfHeight = height * 0.5f;

            // 三角形带四边形顶点顺序（Z形）
            vertices = {
                {{-halfWidth, -halfHeight, 0.0f}, params.color, {0.0f, 0.0f}},
                {{-halfWidth, halfHeight, 0.0f}, params.color, {0.0f, params.uvScale.y}},
                {{halfWidth, -halfHeight, 0.0f}, params.color, {params.uvScale.x, 0.0f}},
                {{halfWidth, halfHeight, 0.0f},
                 params.color,
                 {params.uvScale.x, params.uvScale.y}}};

            // 转换为三角形列表：两个三角形
            indices = {0, 1, 2, 2, 1, 3};

            return {vertices, indices};
        }

        // ==================== 三角形扇形圆（转换为三角形列表） ====================
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        generateTriangleFanCircleAsTriangles(const PrimitiveParams &params)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            float radius = params.param1;
            int segments = params.subdivisions;

            // 中心点
            vertices.push_back({{0.0f, 0.0f, 0.0f}, params.color, {0.5f, 0.5f}});

            // 圆环上的点
            for (int i = 0; i <= segments; ++i)
            {
                float angle = static_cast<float>(i) / static_cast<float>(segments) *
                              2.0f * std::numbers::pi_v<float>;
                float sinAngle = sin(angle);
                float cosAngle = cos(angle);

                Vertex vertex{};
                vertex.pos = {radius * cosAngle, radius * sinAngle, 0.0f};
                vertex.color = params.color;
                vertex.texCoord = {cosAngle * 0.5f + 0.5f, sinAngle * 0.5f + 0.5f};

                vertices.push_back(vertex);
            }

            // 转换为三角形列表
            for (int i = 1; i <= segments; ++i)
            {
                indices.push_back(0); // 中心点
                indices.push_back(i);
                indices.push_back(i + 1);
            }

            return {vertices, indices};
        }
    };

    // 在mainLoop函数前添加这个辅助函数
    const char *getPrimitiveTypeName(primitives::PrimitiveType type)
    {
        switch (type)
        {
        case primitives::PrimitiveType::SPHERE:
            return "Sphere";
        case primitives::PrimitiveType::CUBE:
            return "Cube";
        case primitives::PrimitiveType::PLANE:
            return "Plane";
        case primitives::PrimitiveType::CYLINDER:
            return "Cylinder";
        case primitives::PrimitiveType::CONE:
            return "Cone";
        case primitives::PrimitiveType::TORUS:
            return "Torus";
        case primitives::PrimitiveType::GRID:
            return "Grid";
        case primitives::PrimitiveType::LINE_CUBE:
            return "Line Cube";
        case primitives::PrimitiveType::AXES:
            return "Axes";
        case primitives::PrimitiveType::SIN_WAVE:
            return "Sin Wave";
        case primitives::PrimitiveType::POINTS:
            return "Points";
        case primitives::PrimitiveType::TRIANGLE_STRIP_QUAD:
            return "Triangle Strip Quad";
        case primitives::PrimitiveType::TRIANGLE_FAN_CIRCLE:
            return "Triangle Fan Circle";
        default:
            return "Unknown";
        }
    }
}; // namespace primitives
// NOLINTEND