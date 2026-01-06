#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;// 画布分辨率（宽度,高度）
uniform float u_time;// 时间（从shader启动开始计算，单位：秒）

// 定义两种颜色
vec3 colorA=vec3(.149,.141,.912);// 深蓝色 (R=0.149, G=0.141, B=0.912)
vec3 colorB=vec3(1.,.833,.224);// 金黄色 (R=1.0, G=0.833, B=0.224)

/*
NOTE:
关键概念总结：
时间动画：u_time使shader动起来
周期函数：sin()创建平滑循环
颜色混合：mix()线性插值两个颜色
范围映射：通过数学函数将值映射到需要的范围
*/
void main(){
    vec3 color=vec3(0.);
    
    float pct=abs(sin(u_time));
    
    // 可视化混合比例 pct
    // 在屏幕顶部显示一个进度条
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    if(st.y>.95){
        // 进度条背景
        color=vec3(.3);
        // 进度条前景
        if(st.x<pct){
            color=vec3(1.,0.,0.);// 红色进度
        }
    }else{
        // 原来的颜色混合
        color=mix(colorA,colorB,pct);
    }
    
    // 在屏幕中央显示当前pct值
    if(abs(st.x-.5)<.01&&abs(st.y-.5)<.01){
        color=vec3(pct);// 用灰度显示pct值
    }
    
    gl_FragColor=vec4(color,1.);
}