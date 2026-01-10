#version 450

// 输入：填充色和透明度
layout(location=0)in vec3 fragFillColor;
layout(location=2)in float fragAlpha;

// 输出
layout(location=0)out vec4 outColor;

void main(){
    // 只使用填充色，忽略线框色
    outColor=vec4(fragFillColor,fragAlpha);
}