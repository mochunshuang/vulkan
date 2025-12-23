// 顶点着色器输入
struct VSInput {
    float3 inPosition : POSITION;
    float4 inColor : COLOR;
    float2 inTexCoord : TEXCOORD0;
    float inDistance : TEXCOORD1;    // 新增：到边界的距离
    int inRegionId : TEXCOORD2;      // 新增：区域ID
};

// 顶点着色器输出
struct VSOutput {
    float4 pos : SV_Position;
    float4 fragColor : COLOR;
    float2 fragTexCoord : TEXCOORD0;
    float fragDistance : TEXCOORD1;  // 传递距离
    int fragRegionId : TEXCOORD2;    // 传递区域ID
};

// Uniform缓冲区结构
struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 shadowParams;  // x:内向外(1)或外向内(0), y:渐变强度, z:边界柔化, w:阴影宽度
};

// 常量缓冲区
ConstantBuffer<UniformBuffer> ubo;

// 顶点着色器
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    // 应用MVP变换
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    
    // 传递数据到片段着色器
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.fragDistance = input.inDistance;
    output.fragRegionId = input.inRegionId;
    
    return output;
}

// 片段着色器 - 计算平滑渐变
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target {
    // 使用距离计算平滑渐变
    float distance = vertIn.fragDistance;
    
    // 从uniform获取参数
    bool innerToOuter = (ubo.shadowParams.x > 0.5);
    float gradientStrength = ubo.shadowParams.y;
    float edgeSoftness = ubo.shadowParams.z;
    
    // 计算渐变的alpha值
    float alpha;
    
    if (innerToOuter) {
        // 内向外：从1到0
        alpha = 1.0 - smoothstep(0.0, 1.0, distance);
    } else {
        // 外向内：从0到1
        alpha = smoothstep(0.0, 1.0, distance);
    }
    
    // 应用渐变强度
    alpha *= gradientStrength;
    
    // 添加边缘柔化（可选）
    alpha = smoothstep(0.0, edgeSoftness, alpha);
    
    // 使用白色阴影
    return float4(1.0, 1.0, 1.0, alpha);
}