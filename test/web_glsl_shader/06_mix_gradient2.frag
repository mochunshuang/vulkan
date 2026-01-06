#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

// 定义两种颜色
vec3 colorA=vec3(.149,.141,.912);// 深蓝色
vec3 colorB=vec3(1.,.833,.224);// 金黄色

// 绘制函数曲线的函数
// 在函数值附近绘制一条细线
float plot(vec2 st,float pct){
    return smoothstep(pct-.01,pct,st.y)-
    smoothstep(pct,pct+.01,st.y);
}

#define TEST 1// 控制开关：0=简单线性渐变，1=复杂通道分离

void main(){
    // 归一化坐标 (0.0 到 1.0)
    vec2 st=gl_FragCoord.xy/u_resolution.xy;
    vec3 color=vec3(0.);// 初始黑色
    
    // 创建三维向量pct，每个分量初始为st.x
    // 这意味着一开始所有通道都使用相同的线性渐变
    vec3 pct=vec3(st.x);
    
    // ==============================================
    // TEST开关的影响 - 这是差异的关键所在
    // ==============================================
    #if TEST
    // 当TEST为1时，每个颜色通道使用不同的渐变函数！
    pct.r=smoothstep(0.,1.,st.x);// 红色通道
    pct.g=sin(st.x*PI);// 绿色通道
    pct.b=pow(st.x,.5);// 蓝色通道
    #endif
    // 当TEST为0时，pct.r = pct.g = pct.b = st.x（简单线性）
    
    // ==============================================
    // 颜色混合
    // ==============================================
    // 混合colorA和colorB，但每个通道使用自己的混合比例
    // 这就是颜色变得复杂的原因！
    color=mix(colorA,colorB,pct);
    
    // ==============================================
    // 绘制函数曲线
    // ==============================================
    // 为每个通道绘制对应的函数曲线
    color=mix(color,vec3(1.,0.,0.),plot(st,pct.r));// 红色线
    color=mix(color,vec3(0.,1.,0.),plot(st,pct.g));// 绿色线
    color=mix(color,vec3(0.,0.,1.),plot(st,pct.b));// 蓝色线
    
    gl_FragColor=vec4(color,1.);
}