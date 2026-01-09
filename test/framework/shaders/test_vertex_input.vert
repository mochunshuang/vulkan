#version 450

layout(location=0) in vec2 inPosition;    // 对应 Vertex::pos
layout(location=1) in vec3 inColor;       // 对应 Vertex::color

layout(location=0) out vec3 fragColor;

void main(){
    gl_Position = vec4(inPosition, 0., 1.);  // 直接传递位置
    fragColor = inColor;                     // 直接传递颜色
}