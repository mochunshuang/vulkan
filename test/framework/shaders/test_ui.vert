#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) in vec2 inPosition;
layout(location=1) in vec3 inColor;

layout(location=0) out vec3 fragColor;

// 与3D系统完全相同的Uniform Buffer结构
layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

void main(){
    // 注意：2D UI使用2D位置，z固定为0
    gl_Position = ubo.mvp * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}