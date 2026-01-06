#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    vec3 iResolution;  // 画布分辨率 (width, height, 1.0)
    float iTime;       // 运行时间（秒）
    float iTimeDelta;  // 帧时间差
    float iFrameRate;  // 帧率
    int iFrame;        // 当前帧数
    vec4 iChannelTime; // 通道播放时间
    vec3 iChannelResolution[4]; // 通道分辨率
    vec4 iMouse;       // 鼠标坐标 (x,y,点击状态,0)
    vec4 iDate;        // 日期 (年,月,日,时)
    float iSampleRate; // 音频采样率
} ubo;

//依赖的变量 使用 ubo. 前缀修饰即可

//------------------------------------------------------
float sdRoundRect( in vec2 p, in vec2 b, in float r ) {
    vec2 q = abs(p)-b+r;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r;
}

vec4 normalBlend(vec4 src, vec4 dst) {
    float finalAlpha = src.a + dst.a * (1.0 - src.a);
    return vec4(
        (src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a)) / finalAlpha,
        finalAlpha
    );
}

float sigmoid(float t) {
    return 1.0 / (1.0 + exp(-t));
}

float cornerRadius = 32.0;
float blurRadius = 32.0;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/ubo.iResolution.xy;

    vec2 center = ubo.iResolution.xy / 2.0;
    vec2 hsize = ubo.iResolution.xy / 1.0;
    
	float distShadow = clamp(sigmoid(sdRoundRect(fragCoord - center + vec2(0.0, ubo.iResolution.y / 20.0),
        hsize, cornerRadius + blurRadius) / blurRadius), 0.0, 1.0);
        
    float distRect = clamp(sdRoundRect(fragCoord - center, hsize, cornerRadius), 0.0, 1.0);


    vec3 col = vec3(distShadow);
    col = mix(vec3(1.0), col, distRect);
    
    vec4 finalColor = normalBlend(vec4(vec3(1.0), 0.6), vec4(col, 1.0));
    fragColor = finalColor;
}
//------------------------------------------------------

// 主函数：调用Shadertoy的mainImage
void main() {
    // 关键：将归一化纹理坐标转换为Shadertoy的像素坐标
    vec2 fragCoord = fragTexCoord * ubo.iResolution.xy;
    // 修正后写法（翻转Y轴）
    fragCoord.y = ubo.iResolution.y - fragTexCoord.y * ubo.iResolution.y;
    
    // 调用mainImage，将outColor作为输出颜色传入
    mainImage(outColor, fragCoord);
}