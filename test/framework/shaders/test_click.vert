#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// 修改：push constant传递hover和selected状态
layout(push_constant) uniform PushConstants {
    uint isHovered;   // 悬停状态：0或1
    uint isSelected;  // 选中状态：0或1
} pushConstants;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // 优先级：选中 > 悬停 > 原始颜色
    if (pushConstants.isSelected == 1u) {
        // 选中对象：绿色
        fragColor = vec3(0.0, 1.0, 0.0);
    } else if (pushConstants.isHovered == 1u) {
        // 悬停对象：白色
        fragColor = vec3(1.0, 1.0, 1.0);
    } else {
        // 普通对象：原始颜色
        fragColor = inColor;
    }
}