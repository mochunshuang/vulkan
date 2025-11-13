// 定义包含3个二维向量的顶点位置数组
static float2 positions[3] = float2[](
    float2(0.0, -0.5),  // 顶点0：底部中心 (红色)
    float2(0.5, 0.5),   // 顶点1：右上角 (绿色)
    float2(-0.5, 0.5)   // 顶点2：左上角 (蓝色)
);

// 定义顶点着色器输出结构
struct VertexOutput {
    float3 color;                    // 顶点颜色
    float4 sv_position : SV_Position; // 系统值语义：裁剪空间位置
};

// 定义顶点颜色数组，与位置数组一一对应
static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),  // 红色 - 顶点0
    float3(0.0, 1.0, 0.0),  // 绿色 - 顶点1
    float3(0.0, 0.0, 1.0)   // 蓝色 - 顶点2
);

// 顶点着色器主函数
[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    // 设置顶点位置（从2D扩展到4D齐次坐标）
    // 根据顶点ID从数组获取位置坐标，并转换为齐次坐标
    // xyzw = (x, y, 0.0, 1.0) - 位于z=0平面，w=1表示不进行透视除法
    output.sv_position = float4(positions[vid], 0.0, 1.0);
    // 设置顶点颜色
    output.color = colors[vid];
    return output;
}

// 片段着色器主函数
[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    float3 color = inVert.color;
    // 输出RGBA颜色，alpha值固定为1.0（不透明）
    return float4(color, 1.0);
}
//NOTE: 超过标准化设备坐标 [-1, 1] 范围的片段像素一定被丢弃，这是图形管线的硬性规定，在光栅化阶段就确定，甚至不会调用片段着色器来处理这些不可见区域。