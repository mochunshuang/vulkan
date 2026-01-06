#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 1. 创建网格背景
    vec3 color=vec3(.1);
    
    float grid_density=20.;// 网格密度：20x20
    float grid_line_width=.04;// 线宽（相对比例）
    
    // 网格线
    if(mod(st.x*grid_density,1.)<grid_line_width
    ||
    mod(st.y*grid_density,1.)<grid_line_width){
        color=vec3(.3);
    }
    
    // 2. 绘制正弦函数 y = sin(x)
    // 将x从[0,1]映射到[0, 4π]，这样可以看到两个完整的正弦波
    float x=st.x*4.*PI;
    float y=sin(x);
    
    // 将y值从[-1,1]映射到[0,1]以便在画布上显示
    float display_y=y*.5+.5;
    
    // 3. 绘制正弦曲线（绿色）
    float line_width=.008;
    if(abs(st.y-display_y)<line_width){
        color=vec3(0.,1.,0.);
    }
    
    // 4. 绘制坐标轴
    if(abs(st.x-.5)<.003)color=vec3(.5,0.,0.);// Y轴 - 暗红色
    if(abs(st.y-.5)<.003)color=vec3(0.,.5,0.);// X轴 - 暗绿色
    
    gl_FragColor=vec4(color,1.);
}