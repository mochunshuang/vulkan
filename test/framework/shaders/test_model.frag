#version 450

// 输入来自顶点着色器的颜色
layout(location=0)in vec3 fragColor;

// 输出颜色
layout(location=0)out vec4 outColor;

void main(){
    // 使用顶点颜色
    outColor=vec4(fragColor,1.);
}