#version 450

// 顶点输入：位置、填充色、线框色、透明度
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inFillColor;
layout(location = 2) in vec3 inWireColor;
layout(location = 3) in float inAlpha;

// 输出到片段着色器
layout(location = 0) out vec3 fragFillColor;
layout(location = 1) out vec3 fragWireColor;
layout(location = 2) out float fragAlpha;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragFillColor = inFillColor;
    fragWireColor = inWireColor;
    fragAlpha = inAlpha;
}