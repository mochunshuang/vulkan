// 顶点着色器输入
struct VSInput {
    float2 inPosition : POSITION;
    float3 inColor : COLOR;
};

// 顶点着色器输出
struct VSOutput {
    float4 pos : SV_Position;
    float3 color : COLOR;
};

// Uniform缓冲区结构
struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

// 常量缓冲区
ConstantBuffer<UniformBuffer> ubo;

// 顶点着色器
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 应用MVP变换
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    
    return output;
}

// 片段着色器
[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}