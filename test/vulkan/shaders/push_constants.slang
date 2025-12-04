// 解决方案1：使用float数组代替vec3（推荐）
struct PushConstantData
{
    [[vk::offset(0)]] float2 offset;
    [[vk::offset(8)]] float color[3];  // 使用数组，确保8字节偏移
};

// 顶点着色器输入（保持不变）
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

// 顶点着色器输出（保持不变）
struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

// 片段着色器输入（保持不变）
struct PSInput
{
    float4 pos : SV_POSITION;
    float3 color;
};

// 推送常量声明（保持不变）
[[vk::push_constant]] PushConstantData pushConstants;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 使用推送常量的偏移
    output.pos = float4(input.inPosition + pushConstants.offset, 0.0, 1.0);
    
    // 重建float3颜色
    float3 pushColor = float3(pushConstants.color[0], 
                              pushConstants.color[1], 
                              pushConstants.color[2]);
    
    // 使用推送常量的颜色覆盖输入颜色
    output.color = pushColor;
    
    return output;
}

[shader("fragment")]
float4 fragMain(PSInput input) : SV_TARGET {
    return float4(input.color, 1.0);
}