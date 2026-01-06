
//------------------------------------------------------------------------------
// Shadertoy 到 glsl-canvas 的适配层
#ifdef GL_ES
precision mediump float;
#endif

// 将 glsl-canvas uniforms 映射到 Shadertoy uniforms
uniform vec2 u_resolution;// 对应 iResolution.xy
uniform float u_time;// 对应 iTime
uniform vec2 u_mouse;// 对应 iMouse.xy
uniform vec3 u_camera;// 用于3D效果

// Shadertoy 风格的 uniforms
#define iResolution vec3(u_resolution,1.)
#define iTime u_time
#define iTimeDelta.016// 固定值，可调整
#define iFrameRate 60.0// 固定值，可调整
#define iFrame int(u_time*60.)// 估计值
#define iMouse vec4(u_mouse,0.,0.)// zw部分需要额外处理

// 纹理通道 - 需要在 settings.json 中配置
uniform sampler2D u_texture_0;// 对应 iChannel0
uniform sampler2D u_texture_1;// 对应 iChannel1
uniform sampler2D u_texture_2;// 对应 iChannel2
uniform sampler2D u_texture_3;// 对应 iChannel3

#define iChannel0 u_texture_0
#define iChannel1 u_texture_1
#define iChannel2 u_texture_2
#define iChannel3 u_texture_3

// 日期和时间 - 需要自定义 uniform 或计算
#define iDate vec4(2023.,1.,1.,mod(u_time,86400.))
#define iSampleRate 44100.

// 通道分辨率和时间 - 需要自定义 uniforms
// 可以在 settings.json 中配置：
// "iChannelResolution0": [512.0, 512.0, 1.0]
uniform vec3 iChannelResolution0;
uniform vec3 iChannelResolution1;
uniform vec3 iChannelResolution2;
uniform vec3 iChannelResolution3;

uniform float iChannelTime0;
uniform float iChannelTime1;
uniform float iChannelTime2;
uniform float iChannelTime3;

// Shadertoy 的 mainImage 函数适配
void mainImage(out vec4 fragColor,in vec2 fragCoord);

void main(){
    // 转换为 Shadertoy 的坐标系
    vec2 fragCoord=gl_FragCoord.xy;
    vec4 fragColor;
    
    mainImage(fragColor,fragCoord);
    
    gl_FragColor=fragColor;
}

//------------------------------------------------------------------------------

float Circle(vec2 uv,float r,float blur){
    float d=length(uv);
    float c=smoothstep(r+blur,r-blur,d);
    return c;
}

float Circle2(vec2 uv,vec2 p,float r,float blur){
    //NOTE: -号更符合直觉
    float d=length(uv-p);
    float c=smoothstep(r+blur,r-blur,d);
    return c;
}

void mainImage(out vec4 fragColor,in vec2 fragCoord){
    
    vec2 uv=fragCoord/iResolution.xy;
    
    //NOTE: 1. 下面两个是等价的： 移动圆心
    //c0: 和数字运算，其实是和矢量运算，内置运算符重载
    // uv-=.5;
    uv-=vec2(.5);
    float aspect=iResolution.x/iResolution.y;
    uv.x*=aspect;
    float c=Circle(uv,.3,.001);
    fragColor=vec4(vec3(c),1.);
    
    // NOTE: 2. 等价上面的操作
    uv=fragCoord/iResolution.xy;
    aspect=iResolution.x/iResolution.y;
    uv.x*=aspect;
    vec2 p=vec2(.5);
    p.x*=aspect;// NOTE: 注意：x方向也需要乘以aspect
    c=Circle2(uv,p,.3,.001);
    fragColor=vec4(vec3(c),1.);
    
    //NOTE: 3. 画2个
    uv=fragCoord/iResolution.xy;
    aspect=iResolution.x/iResolution.y;
    uv.x*=aspect;
    p=vec2(.7,.3);
    float r=.3;
    p.x*=aspect;// NOTE: 注意：x方向也需要乘以aspect
    c=Circle2(uv,p,r,.001);
    fragColor=vec4(vec3(c),1.);
    
    p=vec2(.2,.6);
    p.x*=aspect;
    r=.1;
    c+=Circle2(uv,p,r,.001);
    fragColor=vec4(vec3(c),1.);
    
    //NOTE: 4 大圆内部黑色: 得到一双眼睛
    p=vec2(.63,.4);
    p.x*=aspect;
    r=.08;
    c-=Circle2(uv,p,r,.001);
    fragColor=vec4(vec3(c),1.);
    
    p=vec2(.77,.4);
    p.x*=aspect;
    c-=Circle2(uv,p,r,.001);
    fragColor=vec4(vec3(c),1.);
    
    //NOTE: 嘴巴 - 正脸微笑
    // 眼睛位置分析：
    // 左眼：vec2(.63,.4) 右眼：vec2(.77,.4)
    // 眼睛中心线在 y=0.4，嘴巴应该在下方
    
    // 计算嘴巴位置（在两眼中间下方）
    float eyeCenterX=(.63+.77)*.5;// 眼睛中心x坐标
    float mouthY=.2;// 嘴巴在眼睛下方
    
    // 方法1：圆弧形微笑嘴巴（推荐）
    vec2 mouthCenter=vec2(eyeCenterX*aspect,mouthY);
    float mouthRadius=.12;// 嘴巴大小
    
    // 画一个上弧线作为微笑
    // 使用两个圆的差值来创建弧形
    float mouthOuter=Circle2(uv,mouthCenter,mouthRadius,.001);
    float mouthInner=Circle2(uv,mouthCenter+vec2(0.,.02),mouthRadius-.03,.001);
    
    // 只保留下面的弧形部分（形成微笑）
    float smile=0.;
    if(uv.y<mouthY+.05){// 只在嘴巴区域下方绘制
        smile=mouthOuter;
        // 减去内部，形成弧形线条
        smile-=mouthInner;
    }
    
    // 将微笑嘴巴添加到脸上（黑色线条）
    // c-=smile*.5;// 稍微淡一点
    
    // 方法2：简单的微笑线条（更简洁）
    vec2 smileStart=vec2((eyeCenterX-.08)*aspect,mouthY);
    vec2 smileEnd=vec2((eyeCenterX+.08)*aspect,mouthY);
    vec2 smileControl=vec2(eyeCenterX*aspect,mouthY-.05);
    
    // 二次贝塞尔曲线绘制微笑
    float smile2=0.;
    for(float t=0.;t<=1.;t+=.01){
        // 计算贝塞尔曲线上的点
        vec2 point=(1.-t)*(1.-t)*smileStart+2.*(1.-t)*t*smileControl+t*t*smileEnd;
        
        // 计算当前像素到曲线上点的距离
        float d=length(uv-point);
        
        // 绘制曲线
        smile2+=smoothstep(.02,.01,d);
    }
    smile2=clamp(smile2,0.,1.);
    
    // 使用方法2的嘴巴
    c-=smile2;
    
    fragColor=vec4(vec3(c),1.);
}