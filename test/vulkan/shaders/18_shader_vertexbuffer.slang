
// 首先，将顶点着色器更改为不再在着色器代码本身中包含顶点数据。
// 顶点着色器通过以正确的顺序在结构中声明来从顶点缓冲区获取输入。
// 变量inPosition和inColor是顶点属性。它们是在顶点缓冲区中为每个顶点指定的属性，就像我们使用两个数组手动为每个顶点指定位置和颜色一样
// 用来接收和映射缓冲区输入缓冲区
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    // 操作输入的数据，输出最终的顶点
    VSOutput output;
    output.pos = float4(input.inPosition, 0.0, 1.0);
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0); // 接收 VSOutput 并对其着色
}