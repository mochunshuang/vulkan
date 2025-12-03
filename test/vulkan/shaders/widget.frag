#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 transform;
    vec4 color;
    float pointSize;
    float lineWidth;
} pc;

void main() {
    // 对于点图元，使用圆形点
    if (pc.pointSize > 1.0) {
        vec2 coord = gl_PointCoord - vec2(0.5);
        if (length(coord) > 0.5)
            discard;
    }
    
    vec4 texColor = texture(texSampler, fragTexCoord);
    outColor = vec4(fragColor, pc.color.a) * texColor;
}