#version 450

layout(location=0)out vec3 fragColor;

// 主三角形区域
vec2 positions[9] = vec2[](
    vec2(-0.75, 0.25),   // 0: 左上 (第一个三角形)
    vec2(-0.75, -0.25),  // 1: 左下 (第一个三角形)
    vec2(-0.25, 0.0),    // 2: 右中 (第一个三角形)
    
    vec2(0.25, 0.25),    // 3: 左上 (第二个三角形)
    vec2(0.25, -0.25),   // 4: 左下 (第二个三角形)
    vec2(0.75, 0.0),     // 5: 右中 (第二个三角形)

    vec2(0.8, 0.1),      // 6: 小三角形左上
    vec2(0.8, -0.1),     // 7: 小三角形左下
    vec2(0.95, 0.0)      // 8: 小三角形右中
);

// 伪随机数生成器（基于索引）
float random(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return float(seed) / 4294967295.0;
}

// 生成三角形顶点位置（集中在中心上方区域）
vec2 generateRandomPosition(uint index) {
    if (index < 9u) {
        return positions[index];
    }
    
    // 为索引9-20生成随机三角形（最多21个顶点）
    // 三角形集中在中心上方区域：X轴在[-0.5, 0.5]，Y轴在[0.0, 0.8]
    
    // 基于索引确定三角形编号
    uint triangleNum = (index - 9u) / 3u;  // 第几个额外三角形
    uint vertexInTriangle = (index - 9u) % 3u; // 三角形内的顶点索引
    
    // 每个三角形的基础位置
    float baseY = 0.15 + random(triangleNum * 100u) * 0.5;  // Y在[0.15, 0.65]
    float baseX = -0.3 + random(triangleNum * 200u) * 0.6;  // X在[-0.3, 0.3]
    
    float triangleSize = 0.08 + random(triangleNum * 300u) * 0.1; // 三角形大小
    
    // 生成三角形顶点
    if (vertexInTriangle == 0u) {
        // 顶点A：左上
        return vec2(baseX - triangleSize, baseY + triangleSize);
    } else if (vertexInTriangle == 1u) {
        // 顶点B：左下
        return vec2(baseX - triangleSize, baseY - triangleSize);
    } else {
        // 顶点C：右中
        return vec2(baseX + triangleSize, baseY);
    }
}

// 生成动态颜色
vec3 generateColor(uint index) {
    if (index < 9u) {
        // 前9个顶点使用固定颜色
        if (index == 0u) return vec3(1.0, 0.0, 0.0);
        if (index == 1u) return vec3(0.0, 1.0, 0.0);
        if (index == 2u) return vec3(0.0, 0.0, 1.0);
        if (index == 3u) return vec3(1.0, 1.0, 0.0);
        if (index == 4u) return vec3(1.0, 0.0, 1.0);
        if (index == 5u) return vec3(0.0, 1.0, 1.0);
        if (index == 6u) return vec3(1.0, 1.0, 1.0);
        if (index == 7u) return vec3(0.5, 0.5, 1.0);
        return vec3(1.0, 0.5, 0.0);  // index 8
    }
    
    // 索引9-20：使用彩虹色系
    float hue = random(index * 123u);  // 基于索引生成色调
    float saturation = 0.7 + random(index * 456u) * 0.3;  // 饱和度[0.7, 1.0]
    float brightness = 0.8 + random(index * 789u) * 0.2;  // 亮度[0.8, 1.0]
    
    // HSV转RGB
    vec3 hsv = vec3(hue, saturation, brightness);
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}

void main() {
    // 生成位置（支持最多21个顶点）
    vec2 position = generateRandomPosition(gl_VertexIndex);
    
    // 确保位置在有效范围内
    position = clamp(position, vec2(-1.0, -1.0), vec2(1.0, 1.0));
    
    gl_Position = vec4(position, 0.0, 1.0);
    fragColor = generateColor(gl_VertexIndex);
}
