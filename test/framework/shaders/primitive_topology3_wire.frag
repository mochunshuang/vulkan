#version 450

// 输入：线框色和透明度
layout(location=1)in vec3 fragWireColor;
layout(location=2)in float fragAlpha;

// 输出
layout(location=0)out vec4 outColor;

void main(){
    // 只使用线框色，忽略填充色
    outColor=vec4(fragWireColor,fragAlpha);
}