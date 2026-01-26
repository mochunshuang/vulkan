#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// 新增：push constant传递hover状态
layout(push_constant) uniform PushConstants {
    uint isHovered; // 悬停状态：0或1
} pushConstants;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // 如果hovered，输出白色，否则输出原始颜色
    if (pushConstants.isHovered == 1u) {
        fragColor = vec3(1.0, 1.0, 1.0);
    } else {
        fragColor = inColor;
    }
}