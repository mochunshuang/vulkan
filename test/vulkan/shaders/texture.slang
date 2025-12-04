// 顶点着色器输入
struct VSInput {
    float2 inPosition : POSITION;
    float3 inColor : COLOR;
    float2 inTexCoord : TEXCOORD0;  // 新增：纹理坐标
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

// 常量缓冲区
ConstantBuffer<UniformBuffer> ubo;

// 顶点着色器
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 应用MVP变换
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));

     // 这里会进行插值，为每个片段生成对应的纹理坐标
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;

    
    return output;
}

Sampler2D texture;

// 片段着色器
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target {
    //  return texture.Sample(vertIn.fragTexCoord);
    return float4(vertIn.fragColor * texture.Sample(vertIn.fragTexCoord).rgb, 1.0);   //使用差值的顶点颜色混合
}