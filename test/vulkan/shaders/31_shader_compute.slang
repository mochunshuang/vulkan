// 顶点着色器输入结构体 - 从应用程序传递到顶点着色器的数据
struct VSInput {
    float2 inPosition;    // 输入顶点位置 (x, y 坐标)
    float4 inColor;       // 输入顶点颜色 (RGBA格式)
};

// 顶点着色器输出结构体 - 从顶点着色器传递到片段着色器的数据
struct VSOutput
{
    float4 pos : SV_Position;     // 齐次裁剪空间中的位置，系统值语义
    float pointSize : SV_PointSize; // 点精灵的大小，系统值语义
    float3 fragColor : COLOR0;    // 传递给片段着色器的颜色值 (RGB)
};

// 片段着色器输入结构体 - 从顶点着色器输出插值后得到的数据
struct PSInput
{
    float4 pos : SV_POSITION;     // 片段在齐次裁剪空间中的位置
    float3 fragColor : COLOR0;    // 插值后的颜色值
    float2 pointCoord : SV_PointCoord; // 点精灵内的坐标 [0,1]x[0,1]，系统值语义
};

// 顶点着色器主函数
// [shader("vertex")] 属性标识这是一个顶点着色器
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pointSize = 14.0;  // 设置点精灵大小为14像素
    // 将2D位置转换为齐次坐标 (x, y, z=1.0, w=1.0)
    // 这里假设输入位置已经在[-1,1]的标准化设备坐标中
    output.pos = float4(input.inPosition, 1.0, 1.0);
    // 提取RGB颜色分量，忽略Alpha通道
    output.fragColor = input.inColor.rgb;
    // 注释掉的代码：将位置转换为纹理坐标的示例
    //output.pointCoord = (input.inPosition * float2(1.0, -1.0) + float2(1.0)) / 2.0;
    return output;
}

// 片段着色器主函数
// [shader("fragment")] 属性标识这是一个片段着色器
// SV_TARGET 语义表示输出到渲染目标
[shader("fragment")]
float4 fragMain(PSInput input) : SV_TARGET {
    // 将点精灵坐标从[0,1]转换到[-0.5,0.5]，使原点在中心
    float2 coord = input.pointCoord - float2(0.5);
    // 计算当前片段到点精灵中心的距离
    // 使用距离来创建圆形透明度效果：中心不透明，边缘透明
    // 0.5 - length(coord) 意味着在半径0.5处完全透明
    return float4(input.fragColor, 0.5 - length(coord));
}

// 粒子数据结构体 - 表示单个粒子的属性
struct Particle {
	float2 position;   // 粒子位置 (x, y)
	float2 velocity;   // 粒子速度 (vx, vy)
    float4 color;      // 粒子颜色 (RGBA)
};

// 统一缓冲区结构体 - 包含每帧更新的统一数据
struct UniformBuffer {
    float deltaTime;   // 帧时间差，用于物理计算
};

// 声明统一缓冲区常量缓冲区
ConstantBuffer<UniformBuffer> ubo;

// 粒子SSBO包装结构体 - 用于着色器存储缓冲区的粒子数据
struct ParticleSSBO {
    Particle particles;  // 单个粒子数据
};

// 输入结构化缓冲区 - 只读，包含当前帧的粒子状态
StructuredBuffer<ParticleSSBO> particlesIn;

// 输出结构化缓冲区 - 可读写，用于写入下一帧的粒子状态
RWStructuredBuffer<ParticleSSBO> particlesOut;

// 计算着色器主函数
// [shader("compute")] 属性标识这是一个计算着色器
// [numthreads(256,1,1)] 定义线程组维度：每个线程组包含256个线程
[shader("compute")]
[numthreads(256,1,1)]
void compMain(uint3 threadId : SV_DispatchThreadID)  // 系统值：全局线程ID
{
    uint index = threadId.x;  // 获取粒子索引（使用x分量）

    // 更新粒子位置：基于速度和时间积分
    particlesOut[index].particles.position = particlesIn[index].particles.position + 
                                           particlesIn[index].particles.velocity.xy * ubo.deltaTime;
    
    // 保持速度不变（可以在这里添加速度变化逻辑）
    particlesOut[index].particles.velocity = particlesIn[index].particles.velocity;

    // 边界碰撞检测与响应：当粒子碰到窗口边界时翻转运动方向
    
    // 水平边界检测 (x轴边界为 -1.0 和 1.0)
    if ((particlesOut[index].particles.position.x <= -1.0) || 
        (particlesOut[index].particles.position.x >= 1.0)) {
        particlesOut[index].particles.velocity.x = -particlesOut[index].particles.velocity.x;
    }
    
    // 垂直边界检测 (y轴边界为 -1.0 和 1.0)
    if ((particlesOut[index].particles.position.y <= -1.0) || 
        (particlesOut[index].particles.position.y >= 1.0)) {
        particlesOut[index].particles.velocity.y = -particlesOut[index].particles.velocity.y;
    }
}