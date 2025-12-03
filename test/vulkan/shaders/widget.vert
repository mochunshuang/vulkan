#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 transform;
    vec4 color;
    float pointSize;
    float lineWidth;
} pc;

void main() {
    // 应用widget特定的变换和全局变换
    mat4 finalTransform = ubo.proj * ubo.view * ubo.model * pc.transform;
    gl_Position = finalTransform * vec4(inPosition, 1.0);
    gl_PointSize = pc.pointSize;
    fragColor = inColor * pc.color.rgb;
    fragTexCoord = inTexCoord;
}