#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

// 绘制一条线的函数：当输入点(st)的y值等于st.x时绘制绿色线
float plot(vec2 st){
    return smoothstep(.02,0.,abs(st.y-st.x));
}

void main(){
    // 规范化坐标到[0,1]范围
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 设置y值为x值，这样我们就得到y = x的线性函数
    float y=st.x;
    
    // 基础颜色：灰度渐变，从黑(0,0,0)到白(1,1,1)
    vec3 color=vec3(y);
    
    // 调用plot函数计算当前像素是否在线上
    float pct=plot(st);
    
    // 混合颜色：如果不在线上使用灰色，在线上使用绿色
    color=(1.-pct)*color+pct*vec3(0.,1.,0.);
    
    gl_FragColor=vec4(color,1.);
}