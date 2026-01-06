#ifdef GL_ES
precision mediump float;
#endif

uniform float u_time;// 时间（从shader启动开始计算，单位：秒）

// 定义两种颜色
vec3 colorA=vec3(.149,.141,.912);// 深蓝色 (R=0.149, G=0.141, B=0.912)
vec3 colorB=vec3(1.,.833,.224);// 金黄色 (R=1.0, G=0.833, B=0.224)

void main(){
    vec3 color=vec3(0.);// 初始化颜色为黑色
    
    // 1. 计算混合比例 pct（百分比）
    //    sin(u_time) 随时间在[-1, 1]之间波动
    //    abs() 取绝对值，将范围转换为[0, 1]
    float pct=abs(sin(u_time));
    
    // 2. 使用 mix() 函数混合两种颜色
    //    语法：mix(colorA, colorB, pct)
    //    当 pct=0 时，返回 colorA
    //    当 pct=1 时，返回 colorB
    //    当 pct=0.5时，返回两种颜色各50%的混合
    color=mix(colorA,colorB,pct);
    
    gl_FragColor=vec4(color,1.);// 输出最终颜色（完全不透明）
}