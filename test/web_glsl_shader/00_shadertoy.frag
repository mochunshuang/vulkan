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

// Shadertoy 风格的着色器
void mainImage(out vec4 fragColor,in vec2 fragCoord){
    vec2 uv=fragCoord/iResolution.xy;
    
    // 简单的波纹效果
    float wave=sin(uv.x*10.+iTime)*.5+.5;
    wave+=cos(uv.y*8.+iTime*1.5)*.3;
    
    vec3 color=vec3(uv.x,uv.y,.5)*wave;
    
    fragColor=vec4(color,1.);
}