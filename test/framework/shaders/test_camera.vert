#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

// diff: 新增Uniform Buffer用于相机矩阵
layout(binding = 0) uniform CameraUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} cameraUBO;

void main() {
    // diff: 使用MVP矩阵变换顶点
    gl_Position = cameraUBO.proj * cameraUBO.view * cameraUBO.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}