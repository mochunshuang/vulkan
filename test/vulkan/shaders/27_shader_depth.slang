// 定义顶点着色器输入结构
// slang:struct 表示这是一个Slang语言的结构体定义
struct VSInput {
    float3 inPosition;   // 顶点位置 (x, y, z)
    float3 inColor;      // 顶点颜色 (r, g, b)  
    float2 inTexCoord;   // 纹理坐标 (u, v)
};

// 统一缓冲区对象，包含变换矩阵
// 这些矩阵会在CPU端更新，在着色器中只读
struct UniformBuffer {
    float4x4 model;      // 模型矩阵：局部坐标到世界坐标的变换
    float4x4 view;       // 视图矩阵：世界坐标到相机坐标的变换  
    float4x4 proj;       // 投影矩阵：相机坐标到裁剪坐标的变换
};

// 声明常量缓冲区，绑定UniformBuffer结构
// ConstantBuffer表示这是一个在渲染过程中保持不变的缓冲区
ConstantBuffer<UniformBuffer> ubo;

// 顶点着色器输出结构（也是片段着色器输入结构）
struct VSOutput
{
    float4 pos : SV_Position;        // 齐次裁剪空间位置，系统值语义
    float3 fragColor;                // 传递给片段着色器的颜色
    float2 fragTexCoord;             // 传递给片段着色器的纹理坐标
};

// 顶点着色器主函数
// [shader("vertex")] 属性表明这是一个顶点着色器函数
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 坐标变换流水线：局部坐标 → 世界坐标 → 视图坐标 → 投影坐标
    // 注意矩阵乘法顺序：从右往左应用变换
    // 1. 首先将局部坐标变换到世界坐标：mul(ubo.model, float4(input.inPosition, 1.0))
    // 2. 然后变换到视图坐标：mul(ubo.view, 世界坐标)
    // 3. 最后变换到投影坐标：mul(ubo.proj, 视图坐标)
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    
    // 直接传递颜色和纹理坐标到片段着色器
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    
    return output;
}

// 声明纹理采样器
// Sampler2D 表示一个2D纹理，包含纹理数据和采样状态信息
Sampler2D texture;

// 片段着色器主函数  
// [shader("fragment")] 属性表明这是一个片段着色器函数
// SV_TARGET 语义表示输出到渲染目标
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    // 使用纹理采样器对纹理进行采样
    // Sample() 方法使用插值后的纹理坐标从纹理中获取颜色
    // vertIn.fragTexCoord 是在顶点之间插值后的纹理坐标
   return texture.Sample(vertIn.fragTexCoord);
}