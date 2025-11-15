// 顶点着色器输入结构体
// 定义了从顶点缓冲区读取的顶点数据格式
struct VSInput {
    float2 inPosition;   // 顶点位置 (x, y)
    float3 inColor;      // 顶点颜色 (r, g, b)
    float2 inTexCoord;   // 纹理坐标 (u, v)
};

// 统一缓冲区对象结构体
// 包含了变换矩阵，用于CPU->GPU的数据传递
// 注意：需要与C++代码中的UniformBufferObject结构体内存布局匹配
struct UniformBuffer {
    float4x4 model;      // 模型矩阵：局部空间 -> 世界空间
    float4x4 view;       // 视图矩阵：世界空间 -> 观察空间  
    float4x4 proj;       // 投影矩阵：观察空间 -> 裁剪空间
};

// 声明统一缓冲区常量缓冲区
// 这是一个从CPU传递到GPU的只读缓冲区
ConstantBuffer<UniformBuffer> ubo;

// 顶点着色器输出/片段着色器输入结构体
// 定义了从顶点着色器传递到片段着色器的数据
struct VSOutput
{
    float4 pos : SV_Position;        // 齐次裁剪空间位置 (系统语义)
    float2 fragTexCoord;             // 传递给片段着色器的纹理坐标
};

// 顶点着色器主函数
// [shader("vertex")] 属性标识这是一个顶点着色器函数
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 顶点变换：局部空间 -> 裁剪空间
    // 变换顺序：model -> view -> proj
    // 注意：矩阵乘法顺序与HLSL一致（从右到左）
    float4 worldPos = mul(ubo.model, float4(input.inPosition, 0.0, 1.0));    // 局部 -> 世界
    float4 viewPos = mul(ubo.view, worldPos);                                // 世界 -> 观察
    output.pos = mul(ubo.proj, viewPos);                                     // 观察 -> 裁剪
    
    // 传递纹理坐标到片段着色器
    // 这里会进行插值，为每个片段生成对应的纹理坐标
    output.fragTexCoord = input.inTexCoord;
    
    return output;
}

// 声明2D纹理采样器
// Sampler2D 结合了纹理和采样状态
Sampler2D texture;

// 片段着色器主函数  
// [shader("fragment")] 属性标识这是一个片段着色器函数
// SV_TARGET 系统语义表示输出到渲染目标
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    // 使用纹理坐标对纹理进行采样
    // Sample() 方法使用纹理坐标在纹理中查找对应的颜色值
    // 这会返回一个float4(r, g, b, a)颜色值
   return texture.Sample(vertIn.fragTexCoord);
}
//局部坐标 → [模型矩阵] → 世界坐标 → [视图矩阵] → 观察坐标 → [投影矩阵] → 齐次裁剪坐标