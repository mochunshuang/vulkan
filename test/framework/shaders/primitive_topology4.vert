#version 450

// 顶点输入
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inFillColor;
layout(location = 2) in vec3 inWireColor;
layout(location = 3) in float inAlpha;

// 输出到片段着色器
layout(location = 0) out vec3 fragFillColor;
layout(location = 1) out vec3 fragWireColor;
layout(location = 2) out float fragAlpha;

// 推送常量：控制渲染模式
layout(push_constant) uniform PushConstants {
    int renderMode;        // 0=填充, 1=线框, 2=两者
    float lineWidth;       // 线宽
    vec4 wireColorOverride; // 可选的线框颜色覆盖 (包含alpha)
} push;

void main() {
    // 简单的平面投影（用于2D演示）
    gl_Position = vec4(inPosition, 1.0);
    
    // 传递颜色和透明度
    fragFillColor = inFillColor;
    fragWireColor = inWireColor;
    fragAlpha = inAlpha;
}