
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

// 只能复制没有其他办法模拟了。 shader-toy的没有代码提示很难受
// Shadertoy 风格的着色器
void mainImage(out vec4 fragColor,in vec2 fragCoord){
    
    //NOTE: 0. fragCoord 是屏幕像素坐标 。 iResolution 分辨率
    //NOTE: 归一化： 值的范围是 [0,1]
    vec2 uv=fragCoord/iResolution.xy;//归一化得到 2D 纹理坐标
    
    //NOTE: 1. 可以提供渐变，确定 x==0,y==0,x==1,y==1 在屏幕的方向
    fragColor=vec4(uv.x,0,0,1.);//从左到右变大
    fragColor=vec4(uv.y,0,0,1.);//从下到上变大
    
    //NOTE: 2. 左上角全绿，左下角黑，右下角全红，右上角全黄
    // 混合：左下角黑色 -> 右上角 黄色。 渐变的效果
    fragColor=vec4(uv.x,uv.y,0,1.);// [0,0] -> [1,1]
    
    //NOTE:3. 向量的长度，来映射颜色。左下角黑色，右上角大量白
    float d=length(uv);
    float c=d;
    fragColor=vec4(vec3(c),1.);// c: 0 => 根号2. >1的颜色也是白色
    // fragColor=vec4(vec3(0),1.);//全黑
    // fragColor=vec4(vec3(1),1.);//全白
    // fragColor=vec4(vec3(1.414),1.);//全白
    
    //NOTE: 4. 修改uv 。值映射 [0,1] -> [-0.5,0.5]
    uv-=.5;
    d=length(uv);
    c=d;
    fragColor=vec4(vec3(c),1.);//NOTE: 中心全黑，四个角白色
    
    //NOTE: 5. 画圆. 长方形则画出来是椭圆，正方形才是圆。拉伸就看到效果
    if(d<.3)c=1.;
    else c=.0;
    fragColor=vec4(vec3(c),1.);
    
    //NOTE: 6. 补偿得到正规不受拉伸影响的圆
    //NOTE: 使用固定大小的圆，就不应该基于 uv 实现，因为uv受到分辨率影响
    float aspect=iResolution.x/iResolution.y;
    uv.x*=aspect;//NOTE: 保持了 Y轴上的对称
    d=length(uv);
    c=d;
    if(d<.3)c=1.;
    else c=.0;
    fragColor=vec4(vec3(c),1.);//拉伸y。圆会放到或缩小。没有办法
    
    //NOTE: 目前的圆锯齿很大，我们要得到边界光滑的圆。
    uv=fragCoord/iResolution.xy;//归一化得到 2D 纹理坐标
    uv-=.5;
    uv.x*=aspect;//NOTE: 保持了 Y轴上的对称
    d=length(uv);
    // 边缘平滑的圆（抗锯齿）
    float smoothness=.001;//0.1则是模糊
    float radius=.3;
    //https://thebookofshaders.com/glossary/?search=smoothstep
    //NOTE: d 在 radius+smoothness 之前c=0，radius-smoothness 之后c=1
    //NOTE: 否则 c= [radius+smoothness,radius-smoothness] 差值过度
    c=smoothstep(radius+smoothness,radius-smoothness,d);
    fragColor=vec4(vec3(c),1.);
    
}