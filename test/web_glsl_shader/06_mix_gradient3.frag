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

//NOTE: 分通道控制的强大功能，它是创建复杂视觉效果的基础技术！
// 修改代码以可视化每个通道
void main(){
    vec2 st=gl_FragCoord.xy/u_resolution.xy;
    vec3 color=vec3(0.);
    
    vec3 pct=vec3(st.x);
    
    #if TEST
    pct.r=smoothstep(0.,1.,st.x);
    pct.g=sin(st.x*PI);
    pct.b=pow(st.x,.5);
    
    // 实验不同函数组合
    // pct.r=smoothstep(0.,1.,st.x);// 平滑过渡
    // pct.g=fract(st.x*3.);// 锯齿波
    // pct.b=mod(st.x,.3)*3.;// 脉冲波
    
    // 或者
    // pct.r=st.x*st.x;// 二次函数
    // pct.g=st.x*st.x*st.x;// 三次函数
    // pct.b=1.-st.x;// 反相线性
    
    // 或者添加时间动画
    // pct.r=.5+.5*sin(st.x*2.*PI+u_time);
    // pct.g=.5+.5*sin(st.x*4.*PI+u_time*2.);
    // pct.b=.5+.5*sin(st.x*6.*PI+u_time*3.);
    #endif
    
    // 将屏幕分为4个区域查看效果
    if(st.x<.25){
        // 区域1：只看红色通道
        color.r=mix(colorA.r,colorB.r,pct.r);
        color.g=0.;
        color.b=0.;
    }else if(st.x<.5){
        // 区域2：只看绿色通道
        color.r=0.;
        color.g=mix(colorA.g,colorB.g,pct.g);
        color.b=0.;
    }else if(st.x<.75){
        // 区域3：只看蓝色通道
        color.r=0.;
        color.g=0.;
        color.b=mix(colorA.b,colorB.b,pct.b);
    }else{
        // 区域4：完整混合效果
        color=mix(colorA,colorB,pct);
    }
    
    gl_FragColor=vec4(color,1.);
}