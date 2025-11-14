// 顶点着色器输入结构体
struct VSInput {
    float2 inPosition;  // 输入的2D顶点位置 (x, y)
    float3 inColor;     // 输入的顶点颜色 (r, g, b)
};

// 统一缓冲区结构体 - 包含变换矩阵
struct UniformBuffer {
    float4x4 model;     // 模型矩阵：从模型空间变换到世界空间
    float4x4 view;      // 视图矩阵：从世界空间变换到视图/相机空间
    float4x4 proj;      // 投影矩阵：从视图空间变换到裁剪空间
};

// 声明统一缓冲区对象. 这是一个多描述符集
ConstantBuffer<UniformBuffer> ubo;

// 顶点着色器输出结构体（也是片段着色器输入）
struct VSOutput
{
    float4 pos : SV_Position;  // 系统值：变换后的顶点位置（裁剪空间）
    float3 color;              // 传递给片段着色器的颜色值
};

// 顶点着色器主函数
// [shader("vertex")] 属性表明这是顶点着色器
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // MVP矩阵变换：将顶点从模型空间变换到裁剪空间
    // 计算顺序：model -> view -> projection
    // 注意：由于矩阵乘法是从右向左结合，所以先应用model，最后是projection
    // float4(input.inPosition, 0.0, 1.0) 将2D位置扩展为齐次坐标4D向量
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    
    // 将输入颜色直接传递给片段着色器
    output.color = input.inColor;
    
    return output;
}

// 片段着色器主函数
// [shader("fragment")] 属性表明这是片段着色器
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    // 输出最终像素颜色
    // 使用从顶点着色器插值得到的颜色，并添加alpha值1.0（完全不透明）
    return float4(vertIn.color, 1.0);
}