// 顶点着色器输入
struct VSInput {
    float3 inPosition : POSITION;
    float3 inColor : COLOR;
    float2 inTexCoord : TEXCOORD0;
};

// 顶点着色器输出
struct VSOutput {
    float4 pos : SV_Position;
    float3 fragColor : COLOR;
    float2 fragTexCoord : TEXCOORD0;
};

// Uniform缓冲区结构
struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

// 推送常量结构（最大128字节）
struct PushConstants {
    float3 colorModifier;  // 颜色修改器
    float time;            // 时间参数
    float4 userData;       // 用户自定义数据（可选）
};

// 常量缓冲区
ConstantBuffer<UniformBuffer> ubo;

// 推送常量
[[vk::push_constant]] PushConstants pushConstants;

// 顶点着色器
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 应用MVP变换
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));

    // 使用原始顶点颜色
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    
    return output;
}

Sampler2D texture;

// 片段着色器
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target {
    // 应用推送常量的颜色修改
    float3 modifiedColor = vertIn.fragColor * pushConstants.colorModifier;
    
    // 基于时间添加简单的脉冲效果
    float pulse = sin(pushConstants.time * 3.0) * 0.3 + 0.7;
    modifiedColor *= pulse;
    
    // 应用纹理
    float4 texColor = texture.Sample(vertIn.fragTexCoord);
    
    // 使用用户数据控制混合
    float blendFactor = pushConstants.userData.x;
    float3 finalColor = lerp(modifiedColor * texColor.rgb, texColor.rgb, blendFactor);
    
    return float4(finalColor, 1.0);
}