#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

// diff: [new] 添加推送常量块
layout(push_constant) uniform PushConsts {
    mat4 model;
} pushConstants;

layout(location = 0) out vec3 fragColor;

void main() {
    // diff: [new] 应用模型变换
    gl_Position = pushConstants.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}